
#include "main.h"

void del_comm_list(int dir, int block_f, int pe, int fcase,Domain *ld)
{
   int i, j, k, s_len = 0, r_len = 0, n;

   if (fcase >= 10)    /* +- direction encoded in fcase */
      i = fcase - 10;
   else if (fcase >= 0)
      i = fcase;
   else              /* special case to delete the one in a direction when
                        we don't know which quarter we were sending to */
      i = 2;
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
      if (ld->comm_partner[dir][i] == pe)
         break;

   /* i is being used below as an index where information about this
    * block is located */
   ld->num_cases[dir]--;
   for (j = ld->comm_index[dir][i]; j < ld->comm_index[dir][i] + ld->comm_num[dir][i]; j++)
      if (ld->comm_block[dir][j] == block_f && (ld->comm_face_case[dir][j] == fcase ||
          (fcase==  -1 && ld->comm_face_case[dir][j] >= 2 &&
                          ld->comm_face_case[dir][j] <= 5) ||
          (fcase== -11 && ld->comm_face_case[dir][j] >=12 &&
                          ld->comm_face_case[dir][j] <=15))) {
         for (k = j; k < ld->num_cases[dir]; k++) {
            ld->comm_block[dir][k] = ld->comm_block[dir][k+1];
            ld->comm_face_case[dir][k] = ld->comm_face_case[dir][k+1];
            ld->comm_pos[dir][k] = ld->comm_pos[dir][k+1];
            ld->comm_pos1[dir][k] = ld->comm_pos1[dir][k+1];
            ld->comm_send_off[dir][k] = ld->comm_send_off[dir][k+1] - s_len;
            ld->comm_recv_off[dir][k] = ld->comm_recv_off[dir][k+1] - r_len;
         }
         break;
      }
   ld->comm_num[dir][i]--;
   if (ld->comm_num[dir][i]) {
      ld->send_size[dir][i] -= s_len;
      ld->recv_size[dir][i] -= r_len;
      for (j = i+1; j < ld->num_comm_partners[dir]; j++)
         ld->comm_index[dir][j]--;
   } else {
      ld->num_comm_partners[dir]--;
      for (j = i; j < ld->num_comm_partners[dir]; j++) {
         ld->comm_partner[dir][j] = ld->comm_partner[dir][j+1];
         ld->send_size[dir][j] = ld->send_size[dir][j+1];
         ld->recv_size[dir][j] = ld->recv_size[dir][j+1];
         ld->comm_num[dir][j] = ld->comm_num[dir][j+1];
         ld->comm_index[dir][j] = ld->comm_index[dir][j+1] - 1;
      }
   }

   ld->s_buf_num[dir] -= s_len;
   ld->r_buf_num[dir] -= r_len;
}

void update_comm_list(Domain *ld)
{
   int dir, mcp, ncp, mnc, nc, i, j, n, c, f, i1, j1;
   int *cpe, *ci, *cn, *cb, *cf, *cpos, *cpos1;
   int *space = (int *) ld->recv_buff;
   block *bp;

   mcp = ld->num_comm_partners[0];
   if (ld->num_comm_partners[1] > mcp)
      mcp = ld->num_comm_partners[1];
   if (ld->num_comm_partners[2] > mcp)
      mcp = ld->num_comm_partners[2];
   mnc = ld->num_cases[0];
   if (ld->num_cases[1] > mnc)
      mnc = ld->num_cases[1];
   if (ld->num_cases[2] > mnc)
      mnc = ld->num_cases[2];

   cpe = space;
   cn = &cpe[mcp];
   cb = &cn[mnc];
   cf = &cb[mnc];
   cpos = &cf[mnc];
   cpos1 = &cpos[mnc];

   for (dir = 0; dir < 3; dir++) {
      // make copies since original is going to be changed
      ncp = ld->num_comm_partners[dir];
      for (i = 0; i < ncp; i++) {
         cpe[i] = ld->comm_partner[dir][i];
         cn[i] = ld->comm_num[dir][i];
      }
      nc = ld->num_cases[dir];
      for (j = 0; j < nc; j++) {
         cb[j] = ld->comm_block[dir][j];
         cf[j] = ld->comm_face_case[dir][j];
         cpos[j] = ld->comm_pos[dir][j];
         cpos1[j] = ld->comm_pos1[dir][j];
      }

      // Go through communication lists and delete those that are being
      // sent from blocks being moved and change those where the the block
      // being communicated with is being moved (delete if moving here).
      for (n = i = 0; i < ncp; i++)
         for (j = 0; j < cn[i]; j++, n++) {
            bp = &ld->blocks[cb[n]];
            if (bp->new_proc != ld->my_pe)  // block being moved
               del_comm_list(dir, cb[n], cpe[i], cf[n],ld);
            else {
               if (cf[n] >= 10) {
                  f = cf[n] - 10;
                  c = 2*dir + 1;
               } else {
                  f = cf[n];
                  c = 2*dir;
               }
               if (f <= 5) {
                  if (bp->nei[c][0][0] != (-1 - cpe[i])) {
                     del_comm_list(dir, cb[n], cpe[i], cf[n],ld);
                     if ((-1 - bp->nei[c][0][0]) != ld->my_pe)
                        add_comm_list(dir, cb[n], (-1 - bp->nei[c][0][0]),
                                      cf[n], cpos[n], cpos1[n],ld);
                  }
               } else {
                  i1 = (f - 6)/2;
                  j1 = f%2;
                  if (bp->nei[c][i1][j1] != (-1 - cpe[i])) {
                     del_comm_list(dir, cb[n], cpe[i], cf[n],ld);
                     if ((-1 - bp->nei[c][i1][j1]) != ld->my_pe)
                        add_comm_list(dir, cb[n], (-1 - bp->nei[c][i1][j1]),
                                      cf[n], cpos[n], cpos1[n],ld);
                  }
               }
            }
         }
   }
}

void add_par_list(par_comm *pc, int parent, int block, int child, int pe,
                  int sort,Domain *ld)
{
   int i, j, *tmp;

   // first add information into comm_part, comm_num, and index
   // i is being used as an index to where the info goes in the arrays
   for (i = 0; i < pc->num_comm_part; i++)
      if (pc->comm_part[i] >= pe)
         break;

   if (i < pc->num_comm_part && pc->comm_part[i] == pe) {
      for (j = pc->num_comm_part-1; j > i; j--)
         pc->index[j]++;
      pc->comm_num[i]++;
   } else {
      // adding new pe, make sure arrays are large enough
      if (pc->num_comm_part == pc->max_part) {
         pc->max_part = (int)(2.0*((double) (pc->num_comm_part + 1)));
         tmp = (int *) malloc(pc->max_part*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = pc->comm_part[j];
         for (j = i; j < pc->num_comm_part; j++)
            tmp[j+1] = pc->comm_part[j];
         free(pc->comm_part);
         pc->comm_part = tmp;
         tmp = (int *) malloc(pc->max_part*sizeof(int));
         for (j = 0; j < i; j++)
            tmp[j] = pc->comm_num[j];
         for (j = i; j < pc->num_comm_part; j++)
            tmp[j+1] = pc->comm_num[j];
         free(pc->comm_num);
         pc->comm_num = tmp;
         tmp = (int *) malloc(pc->max_part*sizeof(int));
         for (j = 0; j <= i; j++)
            tmp[j] = pc->index[j];
         for (j = i; j < pc->num_comm_part; j++)
            tmp[j+1] = pc->index[j] + 1;
         free(pc->index);
         pc->index = tmp;
      } else {
         for (j = pc->num_comm_part; j > i; j--) {
            pc->comm_part[j] = pc->comm_part[j-1];
            pc->comm_num[j] = pc->comm_num[j-1];
            pc->index[j] = pc->index[j-1] + 1;
         }
      }
      if (i == pc->num_comm_part)
         pc->index[i] = pc->num_cases;
      pc->num_comm_part++;
      pc->comm_part[i] = pe;
      pc->comm_num[i] = 1;
   }

   // now add into to comm_p and comm_b according to index
   // first check if there is room in the arrays
   if (pc->num_cases == pc->max_cases) {
      pc->max_cases = (int)(2.0*((double) (pc->num_cases+1)));
      tmp = (int *) malloc(pc->max_cases*sizeof(int));
      for (j = 0; j < pc->num_cases; j++)
         tmp[j] = pc->comm_p[j];
      free(pc->comm_p);
      pc->comm_p = tmp;
      tmp = (int *) malloc(pc->max_cases*sizeof(int));
      for (j = 0; j < pc->num_cases; j++)
         tmp[j] = pc->comm_b[j];
      free(pc->comm_b);
      pc->comm_b = tmp;
      tmp = (int *) malloc(pc->max_cases*sizeof(int));
      for (j = 0; j < pc->num_cases; j++)
         tmp[j] = pc->comm_c[j];
      free(pc->comm_c);
      pc->comm_c = tmp;
   }
   if (pc->index[i] == pc->num_cases) {
      // at end of arrays
      pc->comm_p[pc->num_cases] = parent;
      pc->comm_b[pc->num_cases] = block;
      pc->comm_c[pc->num_cases] = child;
   } else {
      for (j = pc->num_cases; j >= pc->index[i]+pc->comm_num[i]; j--) {
         pc->comm_p[j] = pc->comm_p[j-1];
         pc->comm_b[j] = pc->comm_b[j-1];
         pc->comm_c[j] = pc->comm_c[j-1];
      }
      for (j = pc->index[i]+pc->comm_num[i]-1; j >= pc->index[i]; j--) {
         if (j == pc->index[i] ||
             (sort && (ld->parents[pc->comm_p[j-1]].number < ld->parents[parent].number
                || (pc->comm_p[j-1] == parent && pc->comm_c[j-1] < child))) ||
             (!sort && (pc->comm_p[j-1] < parent
                || (pc->comm_p[j-1] == parent && pc->comm_c[j-1] < child)))) {
            pc->comm_p[j] = parent;
            pc->comm_b[j] = block;
            pc->comm_c[j] = child;
            break;
         } else {
            pc->comm_p[j] = pc->comm_p[j-1];
            pc->comm_b[j] = pc->comm_b[j-1];
            pc->comm_c[j] = pc->comm_c[j-1];
         }
      }
   }
   pc->num_cases++;
}

void del_par_list(par_comm *pc, int parent, int block, int child, int pe)
{
   int i, j, k;

   // find core number in index list and use i below
   for (i = 0; i < pc->num_comm_part; i++)
      if (pc->comm_part[i] == pe)
         break;

   // find and delete case in comm_p, comm_b, and comm_c
   pc->num_cases--;
   for (j = pc->index[i]; j < pc->index[i]+pc->comm_num[i]; j++)
      if (pc->comm_p[j] == parent && pc->comm_c[j] == child) {
         for (k = j; k < pc->num_cases; k++) {
            pc->comm_p[k] = pc->comm_p[k+1];
            pc->comm_b[k] = pc->comm_b[k+1];
            pc->comm_c[k] = pc->comm_c[k+1];
         }
         break;
      }
   // fix index and adjust comm_part and comm_num
   pc->comm_num[i]--;
   if (pc->comm_num[i])
      for (j = i+1; j < pc->num_comm_part; j++)
         pc->index[j]--;
   else {
      pc->num_comm_part--;
      for (j = i; j < pc->num_comm_part; j++) {
         pc->comm_part[j] = pc->comm_part[j+1];
         pc->comm_num[j] = pc->comm_num[j+1];
         pc->index[j] = pc->index[j+1] - 1;
      }
   }
}

int find_sorted_list(int number, int level,Domain *ld)
{
   int i;

   for (i = ld->sorted_index[level]; i < ld->sorted_index[level+1]; i++)
      if (number == ld->sorted_list[i].number)
         return ld->sorted_list[i].n;
   printf("ERROR: find_sorted_list on %d - number %d not found\n",
          ld->my_pe, number);
   exit(-1);
}


void move_blocks(double *tp, double *tm, double *tu,Domain *ld, unsigned long epoch,int *iter)
{
   static int mul[3][3] = { {1, 2, 0}, {0, 2, 1}, {0, 1, 2} };
   int n, n1, nl, p, c, c1, dir, i, j, k, i1, j1, k1, in,
       offset, off[3], f, fcase, pos[3], proc, number;
   block *bp, *bp1;

   if (ld->stencil == 7)  // add to face case when diags are needed
      f = 0;
   else
      f = 1;

   comm_proc(ld,epoch,*iter);
   (*iter)++;
   comm_parent_proc(ld,epoch,*iter);
   (*iter)++;
   update_comm_list(ld);

   // go through blocks being moved and reset their nei[] list
   // (partially done above with comm_proc) and the lists of their neighbors
   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
      n = ld->sorted_list[in].n;
      if ((bp = &ld->blocks[n])->number >= 0 && bp->new_proc != ld->my_pe) {
         for (c = 0; c < 6; c++) {
            c1 = (c/2)*2 + (c+1)%2;
            dir = c/2;
            fcase = (c1%2)*10;
            if (bp->nei_level[c] == (bp->level-1)) {
               if (bp->nei[c][0][0] >= 0)
                  for (k = fcase+6, i = 0; i < 2; i++)
                     for (j = 0; j < 2; j++, k++)
                        if (ld->blocks[bp->nei[c][0][0]].nei[c1][i][j] == n) {
                           bp1 = &ld->blocks[bp->nei[c][0][0]];
                           offset = ld->p2[ld->num_refine - bp1->level - 1];
                           bp1->nei[c1][i][j] = -1 - bp->new_proc;
                           bp1->nei_refine[c1] = bp->refine;
                           if (bp1->new_proc == ld->my_pe)
                             add_comm_list(dir, bp->nei[c][0][0], bp->new_proc,
                                            k, ((bp1->cen[mul[dir][1]]+(2*i-1)*
                                                offset)*ld->mesh_size[mul[dir][0]]
                                       + bp1->cen[mul[dir][0]]+(2*j-1)*offset),
                                            (bp1->cen[mul[dir][2]] +
                                    (2*(c1%2)-1)*ld->p2[ld->num_refine - bp1->level]),ld);
                           bp->nei_refine[c] = bp1->refine;
                           bp->nei[c][0][0] = -1 - bp1->new_proc;
                           goto done;
                        }
               done: ;
            } else if (bp->nei_level[c] == bp->level) {
               if (bp->nei[c][0][0] >= 0) {
                  bp1 = &ld->blocks[bp->nei[c][0][0]];
                  bp1->nei[c1][0][0] = -1 - bp->new_proc;
                  bp1->nei_refine[c1] = bp->refine;
                  if (bp1->new_proc == ld->my_pe)
                    add_comm_list(dir, bp->nei[c][0][0], bp->new_proc, fcase+f,
                                (bp1->cen[mul[dir][1]]*ld->mesh_size[mul[dir][0]] +
                                    bp1->cen[mul[dir][0]]),
                                   (bp1->cen[mul[dir][2]] +
                                    (2*(c1%2)-1)*ld->p2[ld->num_refine - bp1->level]),ld);
                  bp->nei_refine[c] = bp1->refine;
                  bp->nei[c][0][0] = -1 - bp1->new_proc;
               }
            } else if (bp->nei_level[c] == (bp->level+1)) {
               for (k = fcase+2, i = 0; i < 2; i++)
                  for (j = 0; j < 2; j++, k++)
                     if (bp->nei[c][i][j] >= 0) {
                        bp1 = &ld->blocks[bp->nei[c][i][j]];
                        bp1->nei[c1][0][0] = -1 - bp->new_proc;
                        bp1->nei_refine[c1] = bp->refine;
                        if (bp1->new_proc == ld->my_pe)
                          add_comm_list(dir, bp->nei[c][i][j], bp->new_proc, k,
                                (bp1->cen[mul[dir][1]]*ld->mesh_size[mul[dir][0]] +
                                    bp1->cen[mul[dir][0]]),
                                   (bp1->cen[mul[dir][2]] +
                                    (2*(c1%2)-1)*ld->p2[ld->num_refine- bp1->level]),ld);
                        bp->nei_refine[c] = bp1->refine;
                        bp->nei[c][i][j] = -1 - bp1->new_proc;
                     }
            }
         }
         // move parent connection in blocks being moved
         if (bp->parent != -1) {
            if (bp->parent_node == ld->my_pe) {
               ld->parents[bp->parent].child[bp->child_number] = bp->number;
               ld->parents[bp->parent].child_node[bp->child_number] = bp->new_proc;
               add_par_list(&ld->par_p, bp->parent, bp->number, bp->child_number,
                            bp->new_proc, 1,ld);
               bp->parent = ld->parents[bp->parent].number;
            } else
               del_par_list(&ld->par_b, (-2-bp->parent), n, bp->child_number,
                            bp->parent_node);
         }
      }
   }

   /* swap blocks - if space is tight, may take multiple passes */
   j = 0;
   do {
      exchange(tp, tm, tu,ld,epoch,iter);
      for (n1 = i = 0; i < ld->num_pes; i++)
         n1 += ld->from[i];

      hpx_lco_set(ld->refinelevel,sizeof(int),&n1,HPX_NULL,HPX_NULL);
      hpx_lco_get(ld->refinelevel,sizeof(int),&n);
      j++;
   } while (n && j < 10);

   // reestablish on-core and off-core comm lists
   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
      n = ld->sorted_list[in].n;
      if ((bp = &ld->blocks[n])->number >= 0 && bp->new_proc == -1) {
         nl = bp->number - ld->block_start[bp->level];
         pos[2] = nl/((ld->p2[bp->level]*ld->npx*ld->init_block_x)*
                      (ld->p2[bp->level]*ld->npy*ld->init_block_y));
         pos[1] = (nl%((ld->p2[bp->level]*ld->npx*ld->init_block_x)*
                       (ld->p2[bp->level]*ld->npy*ld->init_block_y)))/
                  (ld->p2[bp->level]*ld->npx*ld->init_block_x);
         pos[0] = nl%(ld->p2[bp->level]*ld->npx*ld->init_block_x);
         for (c = 0; c < 6; c++) {
            dir = c/2;
            i1 = j1 = k1 = 0;
            if      (c == 0) i1 = -1;
            else if (c == 1) i1 =  1;
            else if (c == 2) j1 = -1;
            else if (c == 3) j1 =  1;
            else if (c == 4) k1 = -1;
            else if (c == 5) k1 =  1;
            c1 = (c/2)*2 + (c+1)%2;
            fcase = (c%2)*10;
            if (bp->nei_level[c] == (bp->level-1)) {
               if (bp->nei[c][0][0] < 0) {
                  proc = -1 - bp->nei[c][0][0];
                  i = pos[mul[dir][1]]%2;
                  j = pos[mul[dir][0]]%2;
                  if (proc == ld->my_pe) {
                     number = (((pos[2]/2+k1)*ld->p2[bp->level-1]*ld->npy*ld->init_block_y)+
                                (pos[1]/2+j1))*ld->p2[bp->level-1]*ld->npx*ld->init_block_x+
                              pos[0]/2 + i1 + ld->block_start[bp->level-1];
                     n1 = find_sorted_list(number, (bp->level-1),ld);
                     bp->nei[c][0][0] = n1;
                     ld->blocks[n1].nei[c1][i][j] = n;
                  } else
                     add_comm_list(dir, n, proc, fcase+2+2*i+j,
                                 (bp->cen[mul[dir][1]]*ld->mesh_size[mul[dir][0]] +
                                  bp->cen[mul[dir][0]]),
                                 (bp->cen[mul[dir][2]] +
                                  (2*(c%2)-1)*ld->p2[ld->num_refine- bp->level]),ld);
               }
            } else if (bp->nei_level[c] == bp->level) {
               if (bp->nei[c][0][0] < 0) {
                  proc = -1 - bp->nei[c][0][0];
                  if (proc == ld->my_pe) {
                     number = (((pos[2]+k1)*ld->p2[bp->level]*ld->npy*ld->init_block_y) +
                                (pos[1]+j1))*ld->p2[bp->level]*ld->npx*ld->init_block_x +
                              pos[0] + i1 + ld->block_start[bp->level];
                     n1 = find_sorted_list(number, bp->level,ld);
                     bp->nei[c][0][0] = n1;
                     ld->blocks[n1].nei[c1][0][0] = n;
                  } else
                     add_comm_list(dir, n, proc, fcase+f,
                                 (bp->cen[mul[dir][1]]*ld->mesh_size[mul[dir][0]] +
                                  bp->cen[mul[dir][0]]),
                                 (bp->cen[mul[dir][2]] +
                                  (2*(c%2)-1)*ld->p2[ld->num_refine- bp->level]),ld);
               }
            } else if (bp->nei_level[c] == (bp->level+1)) {
               offset = ld->p2[ld->num_refine - bp->level - 1];
               off[0] = off[1] = off[2] = 0;
               for (k = fcase+6, i = 0; i < 2; i++)
                  for (j = 0; j < 2; j++, k++)
                     if (bp->nei[c][i][j] < 0) {
                        off[mul[dir][0]] = j;
                        off[mul[dir][1]] = i;
                        proc = -1 - bp->nei[c][i][j];
                        if (proc == ld->my_pe) {
                           number = (((2*(pos[2]+k1)-(k1-1)/2+off[2])*
                                          ld->p2[bp->level+1]*ld->npy*ld->init_block_y) +
                                      (2*(pos[1]+j1)-(j1-1)/2+off[1]))*
                                          ld->p2[bp->level+1]*ld->npx*ld->init_block_x +
                                    2*(pos[0]+i1)-(i1-1)/2 + off[0] +
                                    ld->block_start[bp->level+1];
                           n1 = find_sorted_list(number, (bp->level+1),ld);
                           bp->nei[c][i][j] = n1;
                           ld->blocks[n1].nei[c1][0][0] = n;
                        } else
                           add_comm_list(dir, n, proc, k,
                  ((bp->cen[mul[dir][1]]+(2*i-1)*offset)*ld->mesh_size[mul[dir][0]]
                                   + bp->cen[mul[dir][0]]+(2*j-1)*offset),
                                 (bp->cen[mul[dir][2]] +
                                  (2*(c%2)-1)*ld->p2[ld->num_refine- bp->level]),ld);
                     }
            }
         }
         // connect to parent if moved
         if (bp->parent != -1) {
            if (bp->parent_node == ld->my_pe) {
               for (p = 0; p < ld->max_active_parent; p++)
                  if (ld->parents[p].number == -2 - bp->parent) {
                     bp->parent = p;
                     ld->parents[p].child[bp->child_number] = n;
                     ld->parents[p].child_node[bp->child_number] = ld->my_pe;
                     break;
                  }
            } else
               add_par_list(&ld->par_b, (-2-bp->parent), n, bp->child_number,
                            bp->parent_node, 0,ld);
         }
      }
   }
}
