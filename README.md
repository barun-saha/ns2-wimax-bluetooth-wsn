ns2-wimax-bluetooth-wsn
=======================

NS 2.34 source code patched with the modules for WiMax, Bluetooth and WSNs. 

This has been tested with Ubuntu 10.04 on a 32-bit machine, and used with the [ANT Virtual Lab]. Making the patched source code available for public use. However, no guarantee or validation of any sort is provided.

Source of the patches used here:
- WiMax: [ns2-wimax-awg]
- Bluetooth: [UCBT bluetooth module]
- Wireless Sensor Networks: [Mannasim framework]
 
Note that certain changes have to be made in the source code to make them compatible with NS version 2.34. Moreover, the logs are disabled in many case to reduce the output produced. 

[ANT Virtual Lab]: http://virtual-labs.ac.in/cse28/
[ns2-wimax-awg]: https://code.google.com/p/ns2-wimax-awg/
[UCBT bluetooth module]: http://www.cs.uc.edu/~cdmc/ucbt/
[Mannasim framework]: http://www.mannasim.dcc.ufmg.br/
