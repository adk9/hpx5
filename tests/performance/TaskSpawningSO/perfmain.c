#include<stdio.h>
#include "common.h"

int main() 
{
  printf("Starting the HPX Performance test framework\n");
  test_log = fopen("test.log", "w");
  printf("Check test.log for detailed output\n");
  fclose(test_log);
  return 0;
}
