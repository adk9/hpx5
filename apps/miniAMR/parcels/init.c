
#include "main.h"

// Generate check sum for a variable over all active blocks.
double check_sum(int var,Domain *ld)
{
   int n, in, i, j, k;
   double sum, gsum, block_sum;
   block *bp;

   hpx_time_t t1 = hpx_time_now();

   sum = 0.0;
   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
      n = ld->sorted_list[in].n;
      bp = &ld->blocks[n];
      if (bp->number >= 0) {
         block_sum = 0.0;
         for (i = 1; i <= ld->x_block_size; i++)
            for (j = 1; j <= ld->y_block_size; j++)
               for (k = 1; k <= ld->z_block_size; k++)
                  block_sum += bp->array[var][i][j][k];
         sum += block_sum;
      }
   }

   hpx_time_t t2 = hpx_time_now();

   hpx_lco_set(ld->gsum,sizeof(double),&sum,HPX_NULL,HPX_NULL);
   hpx_lco_get(ld->gsum,sizeof(double),&gsum);

   hpx_time_t t3 = hpx_time_now();

   ld->timer_cs_red += hpx_time_diff_ms(t2,t3);
   ld->timer_cs_calc += hpx_time_diff_ms(t1,t2);
   ld->total_red++;

   return gsum;
}


void add_sorted_list(int n, int number, int level,Domain *ld)
{
   int i, j;

   for (i = ld->sorted_index[level]; i < ld->sorted_index[level+1]; i++)
      if (number > ld->sorted_list[i].number)
         break;
   for (j = ld->sorted_index[ld->num_refine+1]; j > i; j--) {
      ld->sorted_list[j].number = ld->sorted_list[j-1].number;
      ld->sorted_list[j].n      = ld->sorted_list[j-1].n;
   }
   ld->sorted_list[i].number = number;
   ld->sorted_list[i].n      = n;
   for (i = level+1; i <= (ld->num_refine+1); i++)
      ld->sorted_index[i]++;
}


// check sizes of send and recv buffers and adjust, if necessary
void check_buff_size(Domain *ld)
{
   int i, j, max_send, max_comm, max_recv;

   for (max_send = max_comm = max_recv = i = 0; i < 3; i++) {
      for (j = 0; j < ld->num_comm_partners[i]; j++)
         if (ld->send_size[i][j] > max_send)
            max_send = ld->send_size[i][j];
      if (ld->num_comm_partners[i] > max_comm)
         max_comm = ld->num_comm_partners[i];
      if (ld->r_buf_num[i] > max_recv)
         max_recv = ld->r_buf_num[i];
   }

   if (max_send > ld->s_buf_size) {
      ld->s_buf_size = (int) (2.0*((double) max_send));
      free(ld->send_buff);
      ld->send_buff = (double *) malloc(ld->s_buf_size*sizeof(double));
   }

   if (max_recv > ld->r_buf_size) {
      ld->r_buf_size = (int) (2.0*((double) max_recv));
      free(ld->recv_buff);
      ld->recv_buff = (double *) malloc(ld->r_buf_size*sizeof(double));
   }

}


// Routines to add and delete entries from the communication list that is
// used to exchange values for ghost cells.
void add_comm_list(int dir, int block_f, int pe, int fcase, int pos, int pos1,Domain *ld)
{
   int i, j, k, s_len = 0, r_len = 0, *tmp, n;

   /* set indexes for send and recieve to determine length of message:
    * for example, if we send a whole face to a quarter face, we will
    * recieve a message sent from a quarter face to a whole face and
    * use 2 as index for the send and 3 for the recv.
    * We can use same index except for offset */
   if (fcase >= 10)    /* +- direction encoded in fcase */
      i = fcase - 10;
   else
      i = fcase;
   switch (i) {
      case 0: s_len = r_len = ld->comm_vars*ld->msg_len[dir][0];
              break;
      case 1: s_len = r_len = ld->comm_vars*ld->msg_len[dir][1];
              break;
      case 2:
      case 3:
      case 4:
      case 5: s_len = ld->comm_vars*ld->msg_len[dir][2];
              r_len = ld->comm_vars*ld->msg_len[dir][3];
              break;
      case 6:
      case 7:
      case 8:
      case 9: s_len = ld->comm_vars*ld->msg_len[dir][3];
              r_len = ld->comm_vars*ld->msg_len[dir][2];
              break;
   }

   for (i = 0; i < ld->num_comm_partners[dir]; i++)
      if (ld->comm_partner[dir][i] >= pe)
         break;

   /* i is being used below as an index where information about this
    * block should go */
   if (i < ld->num_comm_partners[dir] && ld->comm_partner[dir][i] == pe) {
      ld->send_size[dir][i] += s_len;
      ld->recv_size[dir][i] += r_len;
      for (j = ld->num_comm_partners[dir]-1; j > i; j--)
         ld->comm_index[dir][j]++;
      ld->comm_num[dir][i]++;
   } else {
      // make sure arrays are long enough
      // move stuff i and above up one
      if (ld->num_comm_partners[dir] == ld->max_comm_part[dir]) {
         ld->max_comm_part[dir] = (int)(2.0*((double) (ld->num_comm_partners[dir]+1)));
         tmp = (int *) malloc(ld->max_comm_part[dir]*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = ld->comm_partner[dir][j];
         for (j = i; j < ld->num_comm_partners[dir]; j++)
            tmp[j+1] = ld->comm_partner[dir][j];
         free(ld->comm_partner[dir]);
         ld->comm_partner[dir] = tmp;
         tmp = (int *) malloc(ld->max_comm_part[dir]*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = ld->send_size[dir][j];
         for (j = i; j < ld->num_comm_partners[dir]; j++)
            tmp[j+1] = ld->send_size[dir][j];
         free(ld->send_size[dir]);
         ld->send_size[dir] = tmp;
         tmp = (int *) malloc(ld->max_comm_part[dir]*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = ld->recv_size[dir][j];
         for (j = i; j < ld->num_comm_partners[dir]; j++)
            tmp[j+1] = ld->recv_size[dir][j];
         free(ld->recv_size[dir]);
         ld->recv_size[dir] = tmp;
         tmp = (int *) malloc(ld->max_comm_part[dir]*sizeof(int));
         for (j = 0; j <= i; j++)   // Note that this one is different
            tmp[j] = ld->comm_index[dir][j];
         for (j = i; j < ld->num_comm_partners[dir]; j++)
            tmp[j+1] = ld->comm_index[dir][j] + 1;
         free(ld->comm_index[dir]);
         ld->comm_index[dir] = tmp;
         tmp = (int *) malloc(ld->max_comm_part[dir]*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = ld->comm_num[dir][j];
         for (j = i; j < ld->num_comm_partners[dir]; j++)
            tmp[j+1] = ld->comm_num[dir][j];
         free(ld->comm_num[dir]);
         ld->comm_num[dir] = tmp;
      } else {
         for (j = ld->num_comm_partners[dir]; j > i; j--) {
            ld->comm_partner[dir][j] = ld->comm_partner[dir][j-1];
            ld->send_size[dir][j] = ld->send_size[dir][j-1];
            ld->recv_size[dir][j] = ld->recv_size[dir][j-1];
            ld->comm_index[dir][j] = ld->comm_index[dir][j-1] + 1;
            ld->comm_num[dir][j] = ld->comm_num[dir][j-1];
         }
      }

      if (i == ld->num_comm_partners[dir]) {
         if (i == 0)
            ld->comm_index[dir][i] = 0;
         else
            ld->comm_index[dir][i] = ld->comm_index[dir][i-1] + ld->comm_num[dir][i-1];
      }
      ld->num_comm_partners[dir]++;
      ld->comm_partner[dir][i] = pe;
      ld->send_size[dir][i] = s_len;
      ld->recv_size[dir][i] = r_len;
      ld->comm_num[dir][i] = 1;  // still have to put info into arrays
   }

   if ((ld->num_cases[dir]+1) > ld->max_num_cases[dir]) {
      ld->max_num_cases[dir] = (int)(2.0*((double) (ld->num_cases[dir]+1)));
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_block[dir][j];
      free(ld->comm_block[dir]);
      ld->comm_block[dir] = tmp;
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_face_case[dir][j];
      free(ld->comm_face_case[dir]);
      ld->comm_face_case[dir] = tmp;
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_pos[dir][j];
      free(ld->comm_pos[dir]);
      ld->comm_pos[dir] = tmp;
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_pos1[dir][j];
      free(ld->comm_pos1[dir]);
      ld->comm_pos1[dir] = tmp;
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_send_off[dir][j];
      free(ld->comm_send_off[dir]);
      ld->comm_send_off[dir] = tmp;
      tmp = (int *) malloc(ld->max_num_cases[dir]*sizeof(int));
      for (j = 0; j < ld->num_cases[dir]; j++)
         tmp[j] = ld->comm_recv_off[dir][j];
      free(ld->comm_recv_off[dir]);
      ld->comm_recv_off[dir] = tmp;
   }
   if (ld->comm_index[dir][i] == ld->num_cases[dir]) {
      // at end
      ld->comm_block[dir][ld->num_cases[dir]] = block_f;
      ld->comm_face_case[dir][ld->num_cases[dir]] = fcase;
      ld->comm_pos[dir][ld->num_cases[dir]] = pos;
      ld->comm_pos1[dir][ld->num_cases[dir]] = pos1;
      ld->comm_send_off[dir][ld->num_cases[dir]] = ld->s_buf_num[dir];
      ld->comm_recv_off[dir][ld->num_cases[dir]] = ld->r_buf_num[dir];
   } else {
      for (j = ld->num_cases[dir]; j > ld->comm_index[dir][i]+ld->comm_num[dir][i]-1; j--){
         ld->comm_block[dir][j] = ld->comm_block[dir][j-1];
         ld->comm_face_case[dir][j] = ld->comm_face_case[dir][j-1];
         ld->comm_pos[dir][j] = ld->comm_pos[dir][j-1];
         ld->comm_pos1[dir][j] = ld->comm_pos1[dir][j-1];
         ld->comm_send_off[dir][j] = ld->comm_send_off[dir][j-1] + s_len;
         ld->comm_recv_off[dir][j] = ld->comm_recv_off[dir][j-1] + r_len;
      }
      for (j = ld->comm_index[dir][i]+ld->comm_num[dir][i]-1;
           j >= ld->comm_index[dir][i]; j--)
         if (j == ld->comm_index[dir][i] || ld->comm_pos[dir][j-1] < pos ||
             (ld->comm_pos[dir][j-1] == pos && ld->comm_pos1[dir][j-1] < pos1)) {
            ld->comm_block[dir][j] = block_f;
            ld->comm_face_case[dir][j] = fcase;
            ld->comm_pos[dir][j] = pos;
            ld->comm_pos1[dir][j] = pos1;
            if (j == ld->num_cases[dir]) {
               ld->comm_send_off[dir][j] = ld->s_buf_num[dir];
               ld->comm_recv_off[dir][j] = ld->r_buf_num[dir];
            }
            // else comm_[send,recv]_off[j] values are correct
            break;
         } else {
            ld->comm_block[dir][j] = ld->comm_block[dir][j-1];
            ld->comm_face_case[dir][j] = ld->comm_face_case[dir][j-1];
            ld->comm_pos[dir][j] = ld->comm_pos[dir][j-1];
            ld->comm_pos1[dir][j] = ld->comm_pos1[dir][j-1];
            ld->comm_send_off[dir][j] = ld->comm_send_off[dir][j-1] + s_len;
            ld->comm_recv_off[dir][j] = ld->comm_recv_off[dir][j-1] + r_len;
         }
   }
   ld->num_cases[dir]++;
   ld->s_buf_num[dir] += s_len;
   ld->r_buf_num[dir] += r_len;
}

int find_dir(int fact, int npx1, int npy1, int npz1,Domain *ld)
{
   /* Find direction with largest number of processors left
    * that is divisible by the factor.
    */
   int dir;

   if (ld->reorder) {
      if (fact > 2)
         if ((npx1/fact)*fact == npx1)
            if ((npy1/fact)*fact == npy1)
               if ((npz1/fact)*fact == npz1)
                  if (npx1 >= npy1)
                     if (npx1 >= npz1)
                        dir = 0;
                     else
                        dir = 2;
                  else
                     if (npy1 >= npz1)
                        dir = 1;
                     else
                        dir = 2;
               else
                  if (npx1 >= npy1)
                     dir = 0;
                  else
                     dir = 1;
            else
               if (((npz1/fact)*fact) == npz1)
                  if (npx1 >= npz1)
                     dir = 0;
                  else
                     dir = 2;
               else
                  dir = 0;
         else
            if ((npy1/fact)*fact == npy1)
               if (((npz1/fact)*fact) == npz1)
                  if (npy1 >= npz1)
                     dir = 1;
                  else
                     dir = 2;
               else
                  dir = 1;
            else
               dir = 2;
      else /* factor is 2 and np[xyz]1 are either 1 or a factor of 2 */
         if (npx1 >= npy1)
            if (npx1 >= npz1)
               dir = 0;
            else
               dir = 2;
         else
            if (npy1 >= npz1)
               dir = 1;
            else
               dir = 2;
   } else {
      /* if not reorder, divide z fist, y second, and x last */
      if (fact > 2)
         if ((npz1/fact)*fact == npz1)
            dir = 2;
         else if ((npy1/fact)*fact == npy1)
            dir = 1;
         else
            dir = 0;
      else
         if (ld->npz > 1)
            dir = 2;
         else if (ld->npy > 1)
            dir = 1;
         else
            dir = 0;
   }

   return dir;
}


int factor(int np, int *fac)
{
   int nfac = 0, mfac = 2, done = 0;

   while (!done)
      if (np == (np/mfac)*mfac) {
         fac[nfac++] = mfac;
         np /= mfac;
         if (np == 1)
            done = 1;
      } else {
         mfac++;
         if (mfac*mfac > np) {
            fac[nfac++] = np;
            done = 1;
         }
      }

   return nfac;
}


void zero_comm_list(Domain *ld)
{
   int i;

   for (i = 0; i < 3; i++) {
      ld->num_comm_partners[i] = 0;
      ld->s_buf_num[i] = ld->r_buf_num[i] = 0;
      ld->comm_index[i][0] = 0;
      ld->comm_send_off[i][0] = ld->comm_recv_off[i][0] = 0;
   }
}

void init_amr(Domain *ld)
{
  int n, var, i, j, k, l, m, o, size, dir, i1, i2, j1, j2, k1, k2, ib, jb, kb;
   int start[ld->num_pes], pos[3][ld->num_pes], pos1[ld->npx][ld->npy][ld->npz], set,
       num, npx1, npy1, npz1, pes, fact, fac[25], nfac, f;
   block *bp;

   ld->tol = pow(10.0, ((double) -ld->error_tol));

   ld->p2[0] = ld->p8[0] = 1;
   for (i = 0; i < (ld->num_refine+1); i++) {
      ld->p8[i+1] = ld->p8[i]*8;
      ld->p2[i+1] = ld->p2[i]*2;
      ld->sorted_index[i] = 0;
   }
   ld->sorted_index[ld->num_refine+1] = 0;
   ld->block_start[0] = 0;
   ld->global_max_b =  ld->init_block_x*ld->init_block_y*ld->init_block_z;
   num = ld->num_pes*ld->global_max_b;
   for (i = 1; i <= ld->num_refine; i++) {
      ld->block_start[i] = ld->block_start[i-1] + num;
      num *= 8;
      ld->num_blocks[i] = 0;
      ld->local_num_blocks[i] = 0;
   }

   /* initialize for communication arrays, which are initialized below */
   zero_comm_list(ld);

   ld->x_block_half = ld->x_block_size/2;
   ld->y_block_half = ld->y_block_size/2;
   ld->z_block_half = ld->z_block_size/2;

   if (!ld->code) {
      /* for E/W (X dir) messages:
         0: whole -> whole (7), 1: whole -> whole (27),
         2: whole -> quarter, 3: quarter -> whole */
      ld->msg_len[0][0] = ld->msg_len[0][1] = ld->y_block_size*ld->z_block_size;
      ld->msg_len[0][2] = ld->msg_len[0][3] = ld->y_block_half*ld->z_block_half;
      /* for N/S (Y dir) messages */
      ld->msg_len[1][0] = ld->x_block_size*ld->z_block_size;
      ld->msg_len[1][1] = (ld->x_block_size+2)*ld->z_block_size;
      ld->msg_len[1][2] = ld->msg_len[1][3] = ld->x_block_half*ld->z_block_half;
      /* for U/D (Z dir) messages */
      ld->msg_len[2][0] = ld->x_block_size*ld->y_block_size;
      ld->msg_len[2][1] = (ld->x_block_size+2)*(ld->y_block_size+2);
      ld->msg_len[2][2] = ld->msg_len[2][3] = ld->x_block_half*ld->y_block_half;
   } else if (ld->code == 1) {
      /* for E/W (X dir) messages */
      ld->msg_len[0][0] = ld->msg_len[0][1] = (ld->y_block_size+2)*(ld->z_block_size+2);
      ld->msg_len[0][2] = (ld->y_block_half+1)*(ld->z_block_half+1);
      ld->msg_len[0][3] = (ld->y_block_half+2)*(ld->z_block_half+2);
      /* for N/S (Y dir) messages */
      ld->msg_len[1][0] = ld->msg_len[1][1] = (ld->x_block_size+2)*(ld->z_block_size+2);
      ld->msg_len[1][2] = (ld->x_block_half+1)*(ld->z_block_half+1);
      ld->msg_len[1][3] = (ld->x_block_half+2)*(ld->z_block_half+2);
      /* for U/D (Z dir) messages */
      ld->msg_len[2][0] = ld->msg_len[2][1] = (ld->x_block_size+2)*(ld->y_block_size+2);
      ld->msg_len[2][2] = (ld->x_block_half+1)*(ld->y_block_half+1);
      ld->msg_len[2][3] = (ld->x_block_half+2)*(ld->y_block_half+2);
   } else {
      /* for E/W (X dir) messages */
      ld->msg_len[0][0] = ld->msg_len[0][1] = (ld->y_block_size+2)*(ld->z_block_size+2);
      ld->msg_len[0][2] = (ld->y_block_half+1)*(ld->z_block_half+1);
      ld->msg_len[0][3] = (ld->y_block_size+2)*(ld->z_block_size+2);
      /* for N/S (Y dir) messages */
      ld->msg_len[1][0] = ld->msg_len[1][1] = (ld->x_block_size+2)*(ld->z_block_size+2);
      ld->msg_len[1][2] = (ld->x_block_half+1)*(ld->z_block_half+1);
      ld->msg_len[1][3] = (ld->x_block_size+2)*(ld->z_block_size+2);
      /* for U/D (Z dir) messages */
      ld->msg_len[2][0] = ld->msg_len[2][1] = (ld->x_block_size+2)*(ld->y_block_size+2);
      ld->msg_len[2][2] = (ld->x_block_half+1)*(ld->y_block_half+1);
      ld->msg_len[2][3] = (ld->x_block_size+2)*(ld->y_block_size+2);
   }
   npx1 = ld->npx;
   npy1 = ld->npy;
   npz1 = ld->npz;
   for (i = 0; i < 3; i++)
      for (j = 0; j < ld->num_pes; j++)
         pos[i][j] = 0;
   nfac = factor(ld->num_pes, fac);
   ld->max_num_req = ld->num_pes;

   pes = 1;
   start[0] = 0;
   num = ld->num_pes;

   ld->me = (int *) malloc((nfac+1)*sizeof(int));
   ld->np = (int *) malloc((nfac+1)*sizeof(int));
   ld->me[0] = ld->my_pe;
   ld->np[0] = ld->num_pes;
   // initialize
   for (n = 0, i = nfac; i > 0; i--, n++) {
      fact = fac[i-1];
      dir = find_dir(fact, npx1, npy1, npz1,ld);
      if (dir == 0)
         npx1 /= fact;
      else
         if (dir == 1)
            npy1 /= fact;
         else
            npz1 /= fact;
      num /= fact;
      set = ld->me[n]/num;
    // FIXME
    // Allgather/sort here
    printf(" TEST A %d\n",ld->my_pe);
    hpx_lco_allgather_setid(ld->initallgather, ld->my_pe, sizeof(int), &set, HPX_NULL, HPX_NULL);
    hpx_lco_get(ld->initallgather, ld->num_pes*sizeof(int), ld->colors);
    printf(" TEST B %d\n",ld->my_pe);
    //  MPI_Comm_split(comms[n], set, me[n], &comms[n+1]);
    //  MPI_Comm_rank(comms[n+1], &me[n+1]);
    //  MPI_Comm_size(comms[n+1], &np[n+1]);
      for (j = pes-1; j >= 0; j--)
         for (k = 0; k < fact; k++) {
            m = j*fact + k;
            if (!k)
               start[m] = start[j];
            else
               start[m] = start[m-1] + num;
            for (l = start[m], o = 0; o < num; l++, o++)
               pos[dir][l] = pos[dir][l]*fact + k;
         }
      pes *= fact;
   }
   for (i = 0; i < ld->num_pes; i++)
      pos1[pos[0][i]][pos[1][i]][pos[2][i]] = i;

   ld->max_active_block = ld->init_block_x*ld->init_block_y*ld->init_block_z;
   ld->num_active = ld->max_active_block;
   ld->global_active = ld->num_active*ld->num_pes;
   ld->num_parents = ld->max_active_parent = 0;
   size = ld->p2[ld->num_refine+1];  /* block size is p2[num_refine+1-level]
                              * smallest block is size p2[1], so can find
                              * its center */
   ld->mesh_size[0] = ld->npx*ld->init_block_x*size;
   ld->max_mesh_size = ld->mesh_size[0];
   ld->mesh_size[1] = ld->npy*ld->init_block_y*size;
   if (ld->mesh_size[1] > ld->max_mesh_size)
      ld->max_mesh_size = ld->mesh_size[1];
   ld->mesh_size[2] = ld->npz*ld->init_block_z*size;
   if (ld->mesh_size[2] > ld->max_mesh_size)
      ld->max_mesh_size = ld->mesh_size[2];
   if ((ld->num_pes+1) > ld->max_mesh_size)
      ld->max_mesh_size = ld->num_pes + 1;
   ld->bin  = (int *) malloc(ld->max_mesh_size*sizeof(int));
   ld->gbin = (int *) malloc(ld->max_mesh_size*sizeof(int));
   if (ld->stencil == 7)
      f = 0;
   else
      f = 1;
   for (o = n = k1 = k = 0; k < ld->npz; k++)
      for (k2 = 0; k2 < ld->init_block_z; k1++, k2++)
         for (j1 = j = 0; j < ld->npy; j++)
            for (j2 = 0; j2 < ld->init_block_y; j1++, j2++)
               for (i1 = i = 0; i < ld->npx; i++)
                  for (i2 = 0; i2 < ld->init_block_x; i1++, i2++, n++) {
                     m = pos1[i][j][k];
                     if (m == ld->my_pe) {
                        bp = &ld->blocks[o];
                        bp->level = 0;
                        bp->number = n;
                        bp->parent = -1;
                        bp->cen[0] = i1*size + size/2;
                        bp->cen[1] = j1*size + size/2;
                        bp->cen[2] = k1*size + size/2;
                        add_sorted_list(o, n, 0,ld);
                        for (var = 0; var < ld->num_vars; var++)
                           for (ib = 1; ib <= ld->x_block_size; ib++)
                              for (jb = 1; jb <= ld->y_block_size; jb++)
                                 for (kb = 1; kb <= ld->z_block_size; kb++)
                                    bp->array[var][ib][jb][kb] =
                                       ((double) rand())/((double) RAND_MAX);
                        if (i2 == 0)
                           if (i == 0) { /* 0 boundary */
                              bp->nei_level[0] = -2;
                              bp->nei[0][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[0] = 0;
                              bp->nei[0][0][0] = -1 - pos1[i-1][j][k];
                              add_comm_list(0, o, pos1[i-1][j][k], 0+f,
                                            bp->cen[2]*ld->mesh_size[1]+bp->cen[1],
                                            bp->cen[0] - size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[0] = 0;
                           bp->nei[0][0][0] = o - 1;
                        }
                        bp->nei_refine[0] = 0;
                        if (i2 == (ld->init_block_x - 1))
                           if (i == (ld->npx - 1)) { /* 1 boundary */
                              bp->nei_level[1] = -2;
                              bp->nei[1][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[1] = 0;
                              bp->nei[1][0][0] = -1 - pos1[i+1][j][k];
                              add_comm_list(0, o, pos1[i+1][j][k], 10+f,
                                            bp->cen[2]*ld->mesh_size[1]+bp->cen[1],
                                            bp->cen[0] + size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[1] = 0;
                           bp->nei[1][0][0] = o + 1;
                        }
                        bp->nei_refine[1] = 0;
                        if (j2 == 0)
                           if (j == 0) { /* 0 boundary */
                              bp->nei_level[2] = -2;
                              bp->nei[2][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[2] = 0;
                              bp->nei[2][0][0] = -1 - pos1[i][j-1][k];
                              add_comm_list(1, o, pos1[i][j-1][k], 0+f,
                                            bp->cen[2]*ld->mesh_size[0]+bp->cen[0],
                                            bp->cen[1] - size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[2] = 0;
                           bp->nei[2][0][0] = o - ld->init_block_x;
                        }
                        bp->nei_refine[2] = 0;
                        if (j2 == (ld->init_block_y - 1))
                           if (j == (ld->npy - 1)) { /* 1 boundary */
                              bp->nei_level[3] = -2;
                              bp->nei[3][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[3] = 0;
                              bp->nei[3][0][0] = -1 - pos1[i][j+1][k];
                              add_comm_list(1, o, pos1[i][j+1][k], 10+f,
                                            bp->cen[2]*ld->mesh_size[0]+bp->cen[0],
                                            bp->cen[1] + size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[3] = 0;
                           bp->nei[3][0][0] = o + ld->init_block_x;
                        }
                        bp->nei_refine[3] = 0;
                        if (k2 == 0)
                           if (k == 0) { /* 0 boundary */
                              bp->nei_level[4] = -2;
                              bp->nei[4][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[4] = 0;
                              bp->nei[4][0][0] = -1 - pos1[i][j][k-1];
                              add_comm_list(2, o, pos1[i][j][k-1], 0+f,
                                            bp->cen[1]*ld->mesh_size[0]+bp->cen[0],
                                            bp->cen[2] - size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[4] = 0;
                           bp->nei[4][0][0] = o - ld->init_block_x*ld->init_block_y;
                        }
                        bp->nei_refine[4] = 0;
                        if (k2 == (ld->init_block_z - 1))
                           if (k == (ld->npz - 1)) { /* 1 boundary */
                              bp->nei_level[5] = -2;
                              bp->nei[5][0][0] = 0;
                           } else {      /* boundary with neighbor core */
                              bp->nei_level[5] = 0;
                              bp->nei[5][0][0] = -1 - pos1[i][j][k+1];
                              add_comm_list(2, o, pos1[i][j][k+1], 10+f,
                                            bp->cen[1]*ld->mesh_size[0]+bp->cen[0],
                                            bp->cen[2] + size/2,ld);
                           }
                        else {          /* neighbor on core */
                           bp->nei_level[5] = 0;
                           bp->nei[5][0][0] = o + ld->init_block_x*ld->init_block_y;
                        }
                        bp->nei_refine[5] = 0;
                        o++;
                     }
                  }

         check_buff_size(ld);
         for (var = 0; var < ld->num_vars; var++)
           ld->grid_sum[var] = check_sum(var,ld);


}
