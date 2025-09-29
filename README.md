


here is the git just in case: https://github.com/SonnyHernandez90/fuzzy-goggles.git

.


[client.cpp](https://github.com/user-attachments/files/22588716/client.cpp)# fuzzy-goggles
i've attached the excel below 
.
Q1
Using the file sizes 1MB=5.29sec, 10MB=10.37sec, 50MB=31.75, 100MB=58.17sec, 200MB=221.45sec
The visualization shows that the execution time increases roughly linearly with file size but 200 MB case did jump higher than expected transfer time grows with file size


..





Q2
The main bottleneck is the overhead of the FIFO communication act file needed to be split into tiny 256-byte transfers each take up a lot of time
.
below is the excel graph
[timesize.pdf](https://github.com/user-attachments/files/22588697/timesize.pdf)




