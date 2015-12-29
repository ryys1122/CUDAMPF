# CUDAMPF
the source code of our project
==========================================
1. Requirement <br />
OS: Linux <br />
Compilers: GCC, G++, NVCC <br />
CUDA: Version 7.0 <br />
Hardware: device with Compute Capability 3.5 or higher (at least GK110 Kepler architecture) <br />

2. How to use this project <br />
Please make sure the path correct when using makefile to compile whole project. <br />
Please modify the file paths of database and *.hmm in the main_function.cpp manually. <br />
Please use "./integrated_MSV_VIT <1 or 2 or 3 or 4 or 5 or 6 or 7 or 8 or 9> <.hmm file path> <database file path>" in local folder to run the program. <br />
"1" means MSV+local only; "2" means MSV+shared only; "3" means VIT+local only; "4" means VIT+shared only; "5" means SSV+local only; "6" means SSV+shared only; "7" means MSV+switch; "8" means VIT+switch; "9" means SSV+switch; <br />

3. Note <br />
Another version of code with batchIO will be uploaded for loading large database in several times by using streaming, and it is able to fully cover the time consumption of data pre-process. <br />
Database file can be downloaded here: ftp://ftp.ebi.ac.uk/pub/databases/Pfam/releases/Pfam27.0/ <br />

<br /> Any questions about the project please send email to: hjiang5 AT stevens DOT edu <br />

