
#include "main.h"

int refine_level(Domain *ld,unsigned long epoch,int *iter)
{
   int level, nei, n, i, j, k, m, s, t, ix, iy, iz, b, c, c1, change, lchange;
   int unrefine, sib, p, in;
   block *bp, *bp1;
   parent *pp;

   /* block states:
    * 1 block should be refined
    * -1 block could be unrefined
    * 0 block at level 0 and can not be unrefined or
    *         at max level and can not be refined
    */

// get list of neighbor blocks (indirect links in from blocks)

   for (level = ld->cur_max_level; level >= 0; level--) {
      /* check for blocks at this level that will refine
         their neighbors at this level can not unrefine
         their neighbors at a lower level must refine
      */
      do {
         lchange = 0;
         for (in = ld->sorted_index[level]; in < ld->sorted_index[level+1]; in++) {
            n = ld->sorted_list[in].n;
            bp = &ld->blocks[n];
            if (bp->number >= 0) {
               if (bp->level == level && bp->refine == 1) {
                  if (bp->parent != -1 && bp->parent_node == ld->my_pe) {
                     pp = &ld->parents[bp->parent];
                     if (pp->refine == -1)
                        pp->refine = 0;
                     for (b = 0; b < 8; b++)
                        if (pp->child_node[b] == ld->my_pe && pp->child[b] >= 0)
                           if (ld->blocks[pp->child[b]].refine == -1) {
                              ld->blocks[pp->child[b]].refine = 0;
                              lchange++;
                           }
                  }
                  for (i = 0; i < 6; i++)
                     /* neighbors in level above taken care of already */
                     /* neighbors in this level can not unrefine */
                     if (bp->nei_level[i] == level)
                        if ((nei = bp->nei[i][0][0]) >= 0) { /* on core */
                           if (ld->blocks[nei].refine == -1) {
                              ld->blocks[nei].refine = 0;
                              lchange++;
                              if ((p = ld->blocks[nei].parent) != -1 &&
                                    ld->blocks[nei].parent_node == ld->my_pe) {
                                 if ((pp = &ld->parents[p])->refine == -1)
                                    pp->refine = 0;
                                 for (b = 0; b < 8; b++)
                                    if (pp->child_node[b] == ld->my_pe &&
                                        pp->child[b] >= 0)
                                       if (ld->blocks[pp->child[b]].refine == -1) {
                                          ld->blocks[pp->child[b]].refine = 0;
                                          lchange++;
                                       }
                              }
                           }
                        } else { /* off core */
                           if (bp->nei_refine[i] == -1) {
                              bp->nei_refine[i] = 0;
                              lchange++;
                           }
                        }
                     /* neighbors in level below must refine */
                     else if (bp->nei_level[i] == level-1)
                        if ((nei = bp->nei[i][0][0]) >= 0) {
                           if (ld->blocks[nei].refine != 1) {
                              ld->blocks[nei].refine = 1;
                              lchange++;
                           }
                        } else
                           if (bp->nei_refine[i] != 1) {
                              bp->nei_refine[i] = 1;
                              lchange++;
                           }
               }
               if (bp->level == level && bp->refine == -1)
                  // check if block can be unrefined
                  for (i = 0; i < 6; i++)
                     if (bp->nei_level[i] == level+1) {
                        bp->refine = 0;
                        lchange++;
                        if ((p = bp->parent) != -1 &&
                            bp->parent_node == ld->my_pe) {
                           if ((pp = &ld->parents[p])->refine == -1)
                              pp->refine = 0;
                           for (b = 0; b < 8; b++)
                              if (pp->child_node[b] == ld->my_pe &&
                                  pp->child[b] >= 0 &&
                                  ld->blocks[pp->child[b]].refine == -1)
                                 ld->blocks[pp->child[b]].refine = 0;
                        }
                     }
            }
         }
         hpx_lco_set(ld->refinelevel,sizeof(int),&lchange,HPX_NULL,HPX_NULL);
         hpx_lco_get(ld->refinelevel,sizeof(int),&change);

         // Communicate these changes if any made
         if (change) {
          comm_reverse_refine(ld,epoch,*iter);
          (*iter)++;
          // Communicate any changes of which blocks will refine
          comm_refine(ld,epoch,*iter);
          (*iter)++;
          comm_parent_reverse(ld,epoch,*iter);
          (*iter)++;
          comm_parent(ld,epoch,*iter);
          (*iter)++;
         }
      } while (change);

      /* Check for blocks at this level that will remain at this level
         their neighbors at a lower level can not unrefine
      */
      do {
         lchange = 0;
         for (in = ld->sorted_index[level]; in < ld->sorted_index[level+1]; in++) {
            n = ld->sorted_list[in].n;
            bp = &ld->blocks[n];
            if (bp->number >= 0)
               if (bp->level == level && bp->refine == 0)
                  for (c = 0; c < 6; c++)
                     if (bp->nei_level[c] == level-1) {
                        if ((nei = bp->nei[c][0][0]) >= 0) {
                           if (ld->blocks[nei].refine == -1) {
                              ld->blocks[nei].refine = 0;
                              lchange++;
                              if ((p = ld->blocks[nei].parent) != -1 &&
                                    ld->blocks[nei].parent_node == ld->my_pe)
                                 if ((pp = &ld->parents[p])->refine == -1) {
                                    pp->refine = 0;
                                    for (b = 0; b < 8; b++)
                                       if (pp->child_node[b] == ld->my_pe &&
                                           pp->child[b] >= 0 &&
                                           ld->blocks[pp->child[b]].refine == -1)
                                          ld->blocks[pp->child[b]].refine = 0;
                                 }
                           }
                        } else
                           if (bp->nei_refine[c] == -1) {
                              bp->nei_refine[c] = 0;
                              lchange++;
                           }
                     } else if (bp->nei_level[c] == level) {
                        if ((nei = bp->nei[c][0][0]) >= 0)
                           ld->blocks[nei].nei_refine[(c/2)*2+(c+1)%2] = 0;
                     } else if (bp->nei_level[c] == level+1) {
                        c1 = (c/2)*2 + (c+1)%2;
                        for (i = 0; i < 2; i++)
                           for (j = 0; j < 2; j++)
                              if ((nei = bp->nei[c][i][j]) >= 0)
                                 ld->blocks[nei].nei_refine[c1] = 0;
                     }
         }

         hpx_lco_set(ld->refinelevel,sizeof(int),&lchange,HPX_NULL,HPX_NULL);
         hpx_lco_get(ld->refinelevel,sizeof(int),&change);

         // Communicate these changes of any parent that can not refine
         if (change) {
            comm_reverse_refine(ld,epoch,*iter);
            (*iter)++;
            comm_refine(ld,epoch,*iter);
            (*iter)++;
            comm_parent(ld,epoch,*iter);
            (*iter)++;
            // Communicate any changes of which blocks can not unrefine
            comm_parent_reverse(ld,epoch,*iter);
            (*iter)++;
         }
      } while (change);
   }

   for (i = in = 0; in < ld->sorted_index[ld->num_refine+1]; in++)
     if (ld->blocks[ld->sorted_list[in].n].refine == 1)
        i++;

   return(i);
}
