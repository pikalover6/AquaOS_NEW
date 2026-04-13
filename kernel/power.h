#ifndef POWER_H
#define POWER_H

/* System shutdown (ACPI S5). Falls back to CPU halt loop if ACPI fails. */
void power_shutdown(void);

#endif /* POWER_H */
