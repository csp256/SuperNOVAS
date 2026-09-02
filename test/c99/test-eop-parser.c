#include <stdio.h>

#include "novas.h"

int main(void) {
  char url[4096];
  int n = snprintf(url, sizeof(url), "file://%s/EOP-comment-only.txt", RESOURCES);
  if(n < 0 || sizeof(url) <= (size_t) n)
    return 1;

  if(novas_set_eop_url(EOP_RAPID_IAU2000, 2020, url) != -1)
    return 1;

  novas_reset_eop();
  return 0;
}
