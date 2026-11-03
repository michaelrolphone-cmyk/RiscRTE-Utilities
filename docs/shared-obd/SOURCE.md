# Shared OBD production convergence

The published Bringup233e closure was downloaded, SHA-256 checked and decoded by
CI41553516853. X4 production OBD0.2.2 is77504bytes/ecf5c2b7f61bb7de2091c7a2b37c434604dbb640ada51111b8ae9ec19d4e977c.
It matches Utilities a9c6dfea6800676bc272c118cab80c6c41e45c9a and the exact
Runtime/System/Drivers pins in the owning workflow. Production defaults, controls,
preferences schema and paths remain unchanged. Only manifest optional capabilities,
version and unavailable-capability explanation are changed in production source.

The separately installed OBDTest0.2.4 (81280bytes,
9acdf0593b4b3131f4341dbece0e2b178412df16c94abf17bc4657bfd852ec0e) and its
registration/settings/trips remain unchanged. Its experimental motion/landscape,
new default units and recording behavior are not promoted into production.
No physical vehicle, adapter or display testing is claimed.
