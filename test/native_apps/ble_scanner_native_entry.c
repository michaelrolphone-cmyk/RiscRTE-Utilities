/* Test-only observation wrapper; application source is included unchanged. */
#include "../../Apps/ble_scanner.c"
unsigned scanner_test_state(unsigned field){
 switch(field){case 0:return detail;case 1:return selected;case 2:return scroll;case 3:return detail_scroll;case 4:return sensors;case 5:return naming;case 7:return detail_count();case 8:return detail_visible_rows();default:return scan.count;}
}
