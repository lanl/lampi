/*
 * A dummy debug gate to work around a TotalView / ld.so issue.
 *
 * This file is compiled separately and included in the distribution.
 * It may be included in the application's link line to ensure that
 * TotalView will map in the debug gate symbol.
 */

void lampi_debug_gate(void)
{
    extern volatile int MPIR_debug_gate;

    while (MPIR_debug_gate == 0) {
        /* do nothing */;
    }
}
