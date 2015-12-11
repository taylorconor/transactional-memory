#include "stdafx.h"                             // pre-compiled headers
#include <iostream>                             // cout
#include <iomanip>                              // setprecision
#include <random>
#include "helper.h"                             //
#include "BST.h"

using namespace std;                            // cout

#define K           1024                        //
#define GB          (K*K*K)                     //
#define NOPS        100                       //
#define NSECONDS    2                           // run each test for NSECONDS

#define VINT    UINT64                          //  64 bit counter
#define ALIGNED_MALLOC(sz, align) _aligned_malloc(sz, align)
#define GINDX(n)    (g+n*lineSz/sizeof(VINT))   //

UINT64 tstart;                                  // start of test in ms
int lineSz;                                     // cache line size
int maxThread;                                  // max # of threads

THREADH *threadH;                               // thread handles
UINT64 *ops;                                    // for ops per thread

typedef struct {
    int nt;                                     // # threads
    UINT64 rt;                                  // run time (ms)
    UINT64 ops;                                 // ops
} Result;

Result *r;                                      // results
UINT indx;                                      // results index
BST *tree;

volatile VINT *g;                               // NB: position of volatile

class ALIGNEDMA {
	public:
		void *operator new(size_t);
		void operator delete(void *);
};

void *ALIGNEDMA::operator new(size_t sz) {
	sz = (sz + lineSz - 1) / lineSz * lineSz;
	return _aligned_malloc(sz, lineSz);
}

void ALIGNEDMA::operator delete(void *p) {
	_aligned_free(p);
}

UINT64 get_random(int max) {
	std::random_device rd;
	std::default_random_engine gen(rd());
	std::uniform_int_distribution<unsigned long long> dis(0, max);
	return (unsigned long long int) dis(gen);
}

struct ThreadData {
	size_t thread;
	int max_bound;
};

class TestAndSetLock {
	private:
		int lock;

	public:
		TestAndSetLock() {
			lock = 0;
		}

		void acquire() {
			do {
				while (lock == 1) {
					_mm_pause();
				}
			} while (InterlockedExchange(&lock, 1));
		}

		void release() {
			lock = 0;
		}
};

TestAndSetLock *lock;

//
// worker
//
WORKER worker(void *vdata)
{
    ThreadData *data = (ThreadData *)vdata;

    int thread = (int)data->thread;

    UINT64 n = 0;

    volatile VINT *gs = GINDX(maxThread);

    runThreadOnCPU(thread % ncpu);

    while (1) {
       for (int i = 0; i < NOPS; i++) {
	   UINT64 random = get_random(data->max_bound);
	   lock->acquire();
	   tree->Insert((int)random);
	   lock->release();
	   random = get_random(data->max_bound);
	   lock->acquire();
	   tree->Delete((int)random);
	   lock->release();
       }
       n += NOPS*2;

	//
	// check if runtime exceeded
	//
	if ((getWallClockMS() - tstart) > NSECONDS*1000)
		break;

    }
    ops[thread] = n;
    delete data;
    return 0;
	
}

//
// main
//
int main()
{
    ncpu = getNumberOfCPUs();   // number of logical CPUs
    maxThread = 2 * ncpu;       // max number of threads
    lock = new TestAndSetLock();

    //
    // get date
    //
    char dateAndTime[256];
    getDateAndTime(dateAndTime, sizeof(dateAndTime));

    //
    // console output
    //
    cout << getHostName() << " " << getOSName() << " sharing " << (is64bitExe() ? "(64" : "(32") << "bit EXE)" ;
#ifdef _DEBUG
    cout << " DEBUG";
#else
    cout << " RELEASE";
#endif
    cout << " NCPUS=" << ncpu << " RAM=" << (getPhysicalMemSz() + GB - 1) / GB << "GB " << dateAndTime << endl;
#ifdef COUNTER64
    cout << "COUNTER64";
#else
    cout << "COUNTER32";
#endif
    cout << " NOPS=" << NOPS << " NSECONDS=" << NSECONDS;
    cout << endl;
    cout << "Intel" << (cpu64bit() ? "64" : "32") << " family " << cpuFamily() << " model " << cpuModel() << " stepping " << cpuStepping() << " " << cpuBrandString() << endl;

    //
    // get cache info
    //
    lineSz = getCacheLineSz();
    cout << endl;

    //
    // allocate global variable
    //
    // NB: each element in g is stored in a different cache line to stop false sharing
    //
    threadH = (THREADH*) ALIGNED_MALLOC(maxThread*sizeof(THREADH), lineSz);             // thread handles
    ops = (UINT64*) ALIGNED_MALLOC(maxThread*sizeof(UINT64), lineSz);                   // for ops per thread

    g = (VINT*) ALIGNED_MALLOC((maxThread + 1)*lineSz, lineSz);                         // local and shared global variables
    r = (Result*) ALIGNED_MALLOC(5*maxThread*sizeof(Result), lineSz);                   // for results
    memset(r, 0, 5*maxThread*sizeof(Result));                                           // zero

    indx = 0;

    //
    // use thousands comma separator
    //
    setCommaLocale();

    //
    // header
    //
    cout << "nt";
    cout << setw(10) << "range";
    cout << setw(6) << "rt";
    cout << setw(16) << "ops";
    cout << setw(6) << "rel";
    cout << endl;

    cout << "--"; 		    // nt
    cout << setw(10) << "--";        // rt
    cout << setw(6) << "-----";    // range
    cout << setw(16) << "---";      // ops
    cout << setw(6) << "---";       // rel
    cout << endl;

    //
    // run tests
    //
    UINT64 ops1 = 0;
    int range_vals[5] = {16, 256, 4096, 65536, 1048576}; 

    for (int nt = 1; nt <= maxThread; nt++, indx++) {
	for (int range_idx = 0; range_idx < 5; range_idx++) {

            tree = new BST();
	    int range = range_vals[range_idx];
	
	    //
	    //  zero shared memory
	    //
	    for (int thread = 0; thread < nt; thread++)
		    *(GINDX(thread)) = 0;   // thread local
	    *(GINDX(maxThread)) = 0;    // shared


	    //
	    // get start time
	    //
	    tstart = getWallClockMS();

	    //
	    // create worker threads
	    //
	    for (int thread = 0; thread < nt; thread++) {
		    ThreadData *data = (ThreadData*)malloc(sizeof(ThreadData));
		    data->thread = (size_t)thread;
		    data->max_bound = range;
		    createThread(&threadH[thread], worker, (void*)data);
	    }


	    //
	    // wait for ALL worker threads to finish
	    //
	    waitForThreadsToFinish(nt, threadH);
	    UINT64 rt = getWallClockMS() - tstart;

	    delete tree;

	    //
	    // save results and output summary to console
	    //
	    for (int thread = 0; thread < nt; thread++) {
		    r[indx].ops += ops[thread];
	    }
	    if (!ops1)
		    ops1 = r[indx].ops;
	    r[indx].nt = nt;
	    r[indx].rt = rt;

	    cout << setw(2) << nt;
	    cout << setw(10) << range;
	    cout << setw(6) << fixed << setprecision(2) << (double) rt / 1000;
	    cout << setw(16) << r[indx].ops;
	    cout << setw(6) << fixed << setprecision(2) << (double) r[indx].ops / ops1;

	    cout << endl;

	    r[indx].ops = 0;

	    //
	    // delete thread handles
	    //
	    for (int thread = 0; thread < nt; thread++)
		    closeThread(threadH[thread]);
	}
    }

    cout << endl;

    return 0;

}

// eof
