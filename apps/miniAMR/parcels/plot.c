
#include "main.h"

int _plot_result_action(NodalArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // 0. wait for the right generation to become active
  hpx_lco_gencount_wait(ld->epoch, args->epoch);

  int size = args->total_num_blocks;
  int srcIndex = args->srcIndex; 
  int *incoming_buf = args->buf;

  // 1. acquire the domain lock
  hpx_lco_sema_p(ld->sem_plot);

  // 2. update
  ld->plot_buf[srcIndex] = (int *) malloc (4*size*sizeof(int));
  ld->plot_buf_size[srcIndex] = size;

  memcpy(ld->plot_buf[srcIndex], incoming_buf, sizeof(int)*4*size);

  // 3. release the domain lock
  hpx_lco_sema_v(ld->sem_plot);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->plot_and[args->epoch % 2], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}
  

int _plot_sends_action(plotSBN *psbn)
{
  int *buf;
  buf = psbn->buf;
  hpx_addr_t local = hpx_thread_current_target();
  int total_num_blocks = psbn->total_num_blocks;
 
  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(NodalArgs) + 4*total_num_blocks*sizeof(int));
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  NodalArgs *nodal = hpx_parcel_get_data(p);

  nodal->total_num_blocks = total_num_blocks;
  nodal->epoch = psbn->epoch;
  nodal->srcIndex = psbn->rank;
  int i;
  for (i=0;i<4*total_num_blocks;i++) {
    nodal->buf[i] = buf[i];
  }

  // all parcels go to master
  int distance = -psbn->rank;
  hpx_addr_t neighbor = hpx_addr_add(local, sizeof(Domain) * distance);

  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _plot_result);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Write block information (level and center) to plot file.
void plot(int ts,Domain *ld,unsigned long epoch)
{
   int i, j, n, total_num_blocks, *buf, buf_size, size;
   char fname[20];
   block *bp;
   FILE *fp;
  
   // allocate the next generation 
   ld->plot_and[(epoch + 1) % 2] = hpx_lco_and_new(ld->num_pes-1);

   if ( ld->my_pe != 0 ) {
     hpx_addr_t local = hpx_thread_current_target();
     total_num_blocks = 0;
     for (i = 0; i <= ld->num_refine; i++)
        total_num_blocks += ld->local_num_blocks[i];
     buf = (int *) malloc(4*total_num_blocks*sizeof(int));
     for (i = n = 0; n < ld->max_active_block; n++)
        if ((bp = &ld->blocks[n])->number >= 0) {
           buf[i++] = bp->level;
           buf[i++] = bp->cen[0];
           buf[i++] = bp->cen[1];
           buf[i++] = bp->cen[2];
        }

     hpx_addr_t sends = hpx_lco_and_new(1);

     hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(plotSBN));
     assert(p);

     plotSBN *psbn = hpx_parcel_get_data(p);

     psbn->rank             = ld->my_pe;
     psbn->buf              = buf;
     psbn->total_num_blocks = total_num_blocks;
     psbn->epoch            = epoch;

     hpx_parcel_set_target(p, local);
     hpx_parcel_set_action(p, _plot_sends);
     hpx_parcel_set_cont_target(p, sends);
     hpx_parcel_set_cont_action(p, hpx_lco_set_action);

     // async is fine, since we're waiting on sends below
     hpx_parcel_send(p, HPX_NULL);

     hpx_lco_wait(sends); 
     hpx_lco_delete(sends, HPX_NULL);
     free(buf);
   }

   hpx_lco_gencount_inc(ld->epoch, HPX_NULL);
   if (ld->my_pe == 0) {
      hpx_lco_wait(ld->plot_and[epoch % 2]);
      hpx_lco_delete(ld->plot_and[epoch % 2], HPX_NULL);

      fname[0] = 'p';
      fname[1] = 'l';
      fname[2] = 'o';
      fname[3] = 't';
      fname[4] = '.';
      for (n = 1, j = 0; n < ld->num_tsteps; j++, n *= 10) ;
      for (n = 1, i = 0; i <= j; i++, n *= 10)
         fname[5+j-i] = (char) ('0' + (ts/n)%10);
      fname[6+j] = '\0';
      fp = fopen(fname, "w");

      total_num_blocks = 0;
      for (i = 0; i <= ld->num_refine; i++)
         total_num_blocks += ld->num_blocks[i];
      fprintf(fp, "%d %d %d %d %d\n", total_num_blocks, ld->num_refine,
                                      ld->npx*ld->init_block_x, ld->npy*ld->init_block_y,
                                      ld->npz*ld->init_block_z);
      buf_size = 0;
      fprintf(fp, "%d\n", ld->num_active);
      for (n = 0; n < ld->max_active_block; n++)
         if ((bp = &ld->blocks[n])->number >= 0)
            fprintf(fp, "%d %d %d %d\n", bp->level, bp->cen[0],
                                         bp->cen[1], bp->cen[2]);

      for (i = 1; i < ld->num_pes; i++) {
         size = ld->plot_buf_size[i];
         for (n = j = 0; j < size; j++, n += 4)
            fprintf(fp, "%d %d %d %d\n", ld->plot_buf[i][n], ld->plot_buf[i][n+1], ld->plot_buf[i][n+2], ld->plot_buf[i][n+3]);
         free(ld->plot_buf[i]);
      }
      fclose(fp);
   }
   
}
