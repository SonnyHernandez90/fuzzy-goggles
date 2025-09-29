/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:
	UIN:
	Date:
*/
#include "common.h"
#include "FIFORequestChannel.h"

#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>
#include <memory>
#include <algorithm>
#include <cstdio>

using namespace std;

int main (int argc, char *argv[]) {

    int opt;
    int p = -1;
    double t = 0.0;
    int e = -1;
    string filename = "";
    int buffercapacity = MAX_MESSAGE;
    bool use_new_channel = false;

    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p': 
                p = atoi(optarg); 
                break;
            case 't':
                t = atof(optarg); 
                break;
            case 'e':
                e = atoi(optarg); 
                break;
            case 'f':
                filename = optarg; 
                break;
            case 'm':
                buffercapacity = atoi(optarg); 
                break;
            case 'c':
                use_new_channel = true; 
                break;
        }
    }

    if (p != -1 && (p < 1 || p > NUM_PERSONS)) {
        EXITONERROR("patient id must be in [1,15]");
    }
    if (e != -1 && !(e == 1 || e == 2)) {
        EXITONERROR("ecg number must be 1 or 2");
    }
    if (buffercapacity <= 0) {
        EXITONERROR("buffer capacity must be positive");
    }

    // child 
    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        if (buffercapacity != MAX_MESSAGE) {
            char mstr[32];
            snprintf(mstr, sizeof(mstr), "%d", buffercapacity);
            execl("./server", "server", "-m", mstr, (char*)nullptr);
        } else {
            execl("./server", "server", (char*)nullptr);
        }
        perror("execl server");
        _exit(127);
    }

    // connect 
    FIFORequestChannel control("control", FIFORequestChannel::CLIENT_SIDE);

   
    FIFORequestChannel* chan = &control;
    FIFORequestChannel* newchan = nullptr;
    if (use_new_channel) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        control.cwrite(&m, sizeof(m));
        char namebuf[128] = {0};
        int n = control.cread(namebuf, sizeof(namebuf));
        if (n <= 0) EXITONERROR("failed to get new channel name");
        newchan = new FIFORequestChannel(namebuf, FIFORequestChannel::CLIENT_SIDE);
        chan = newchan;
    }

    // helpers 
    auto request_datapoint = [&](int person, double seconds, int ecgno) -> double {
        datamsg dm(person, seconds, ecgno);
        char buf[sizeof(datamsg)];
        memcpy(buf, &dm, sizeof(dm));
        chan->cwrite(buf, sizeof(dm));
        double reply = 0.0;
        if (chan->cread(&reply, sizeof(double)) != (int)sizeof(double)) {
            EXITONERROR("DATA_MSG read failed");
        }
        return reply;
    };

    auto request_filesize = [&](const string& fname) -> __int64_t {
        filemsg fm(0, 0);
        int len = sizeof(filemsg) + (int)fname.size() + 1;
        unique_ptr<char[]> req(new char[len]);
        memcpy(req.get(), &fm, sizeof(filemsg));
        strcpy(req.get() + sizeof(filemsg), fname.c_str());
        chan->cwrite(req.get(), len);
        __int64_t sz = 0;
        if (chan->cread(&sz, sizeof(__int64_t)) != (int)sizeof(__int64_t)) {
            EXITONERROR("FILE size read failed");
        }
        return sz;
    };

    auto ensure_received_dir = [&](){
        if (mkdir("received", 0777) == -1 && errno != EEXIST) {
            perror("mkdir(received)");
            exit(1);
        }
    };

    // modes
    if (!filename.empty()) {
     
        ensure_received_dir();

        __int64_t fsize = request_filesize(filename);
        if (fsize < 0) EXITONERROR("negative file size from server");

        string outpath = "received/" + filename;
        int outfd = open(outpath.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (outfd < 0) {
            perror(("open " + outpath).c_str());
            exit(1);
        }

        unique_ptr<char[]> rbuf(new char[buffercapacity]);
        __int64_t offset = 0;
        while (offset < fsize) {
            int chunk = (int)min<__int64_t>(fsize - offset, buffercapacity);
            filemsg fm(offset, chunk);
            int len = sizeof(filemsg) + (int)filename.size() + 1;
            unique_ptr<char[]> req(new char[len]);
            memcpy(req.get(), &fm, sizeof(filemsg));
            strcpy(req.get() + sizeof(filemsg), filename.c_str());

            chan->cwrite(req.get(), len);
            int n = chan->cread(rbuf.get(), chunk);
            if (n != chunk) EXITONERROR("unexpected FILE chunk size");

            size_t written = 0;
            while (written < (size_t)chunk) {
                ssize_t rc = pwrite(outfd, rbuf.get() + written, (size_t)chunk - written,
                                    (off_t)(offset + (ssize_t)written));
                if (rc < 0) { perror("pwrite"); exit(1); }
                written += (size_t)rc;
            }
            offset += chunk;
        }
        close(outfd);
        
        cout << "Downloaded " << filename << " (" << fsize << " bytes) to " << outpath << endl;
    }
    else if (p != -1 && e != -1) {

        double reply = request_datapoint(p, t, e);
        cout << "For person " << p << ", at time " << t
             << ", the value of ecg " << e << " is " << reply << endl;
    }
    else if (p != -1 && e == -1 && filename.empty()) {

        ensure_received_dir();

        ofstream ofs("received/x1.csv");
        if (!ofs) EXITONERROR("cannot open received/x1.csv");

        for (int i = 0; i < 1000; ++i) {
            double tt = i * 0.004;
            double v1 = request_datapoint(p, tt, 1);
            double v2 = request_datapoint(p, tt, 2);
          
            ofs << tt << "," << v1 << "," << v2 << "\n";
        }
        ofs.close();
        
    }


    // clean shutdown
    if (newchan) {
        MESSAGE_TYPE q = QUIT_MSG;
        newchan->cwrite(&q, sizeof(q));
        delete newchan;
    }
    {
        MESSAGE_TYPE q = QUIT_MSG;
        control.cwrite(&q, sizeof(q));
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    return 0;
}
