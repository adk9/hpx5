#include <libhpx/symbol_table.h>

static int _get_symbol_table_entry_handler(hpx_addr_t addr) {
  struct symbol_table *sym_tab_entry;

  HASH_FIND_INT(here->sym_tab, &addr, sym_tab_entry); 
  
  if (sym_tab_entry!=NULL) {
    hpx_thread_continue(sym_tab_entry->data_type, 
        strlen(sym_tab_entry->data_type));
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(HPX_DEFAULT, 0, _get_symbol_table_entry, 
    _get_symbol_table_entry_handler, HPX_ADDR);

static int _get_symbol_table_entry_length_handler(hpx_addr_t addr) {
  struct symbol_table *sym_tab_entry;
  int length = 0;

  HASH_FIND_INT(here->sym_tab, &addr, sym_tab_entry);
  
  if (sym_tab_entry) {
    length = strlen(sym_tab_entry->data_type);
    hpx_thread_continue(&length, sizeof(length));
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(HPX_DEFAULT, 0, _get_symbol_table_entry_length, 
    _get_symbol_table_entry_length_handler, HPX_ADDR);

void symbol_table_add(hpx_addr_t addr, char *data_type) {
  struct symbol_table *sym_tab_entry;

  HASH_FIND_INT(here->sym_tab, &addr, sym_tab_entry);
  if (sym_tab_entry==NULL) {
    sym_tab_entry = (struct symbol_table*)malloc(sizeof(struct symbol_table));
    sym_tab_entry->addr = addr;
    sym_tab_entry->data_type = (char *)malloc(strlen(data_type));
    HASH_ADD_INT(here->sym_tab, addr, sym_tab_entry);
  }
  strncpy(sym_tab_entry->data_type, data_type, strlen(data_type));
}

struct symbol_table *symbol_table_find(hpx_addr_t addr) {
  struct symbol_table *sym_tab_entry;
  int sym_tab_rank = gpa_to_rank(addr);
  int length = 0;
  if (sym_tab_rank==here->rank) {
    HASH_FIND_INT(here->sym_tab, &addr, sym_tab_entry);
  }
  else {
    hpx_addr_t rank_addr = HPX_THERE(sym_tab_rank);
    hpx_call_sync(rank_addr, _get_symbol_table_entry_length, &length,
        sizeof(length), &addr);
    sym_tab_entry = (struct symbol_table*)malloc(sizeof(struct symbol_table));
    sym_tab_entry->addr = addr;
    sym_tab_entry->data_type = (char *) malloc(length);
    HASH_ADD_INT(here->sym_tab, addr, sym_tab_entry);
    hpx_call_sync(rank_addr, _get_symbol_table_entry, 
        sym_tab_entry->data_type, length, &addr);
  }
  return sym_tab_entry;
}

void symbol_table_delete(struct symbol_table *sym_tab_entry) {
  HASH_DEL(here->sym_tab, sym_tab_entry);
  free(sym_tab_entry->data_type);
  free(sym_tab_entry);
}

void symbol_table_delete_all() {
  struct symbol_table *curr_entry, *temp_entry;

  HASH_ITER(hh, here->sym_tab, curr_entry, temp_entry) {
    HASH_DEL(here->sym_tab,curr_entry);
    free(curr_entry->data_type);
    free(curr_entry);
  }
}

int addr_sort(struct symbol_table *a, struct symbol_table *b) {
  return (a->addr - b->addr);
}

void symbol_table_sort() {
  HASH_SORT(here->sym_tab, addr_sort);
}


