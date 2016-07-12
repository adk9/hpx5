#include <hpx/hpx.h>
#include <uthash.h>
#include <libhpx/locality.h>
#include <libhpx/gpa.h>

struct symbol_table {
  hpx_addr_t addr;
  char *data_type;
  UT_hash_handle hh;
};

void symbol_table_add(hpx_addr_t addr, char *data_type);

struct symbol_table *symbol_table_find(hpx_addr_t addr);

void symbol_table_delete(struct symbol_table *sym_tab_entry);

void symbol_table_delete_all();

void symbol_table_print();

int addr_sort(struct symbol_table *a, struct symbol_table *b);

void symbol_table_sort();

