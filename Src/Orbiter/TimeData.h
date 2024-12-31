#ifndef TIMEDATA_H
#define TIMEDATA_H

#include <cstddef>
//-----------------------------------------------------------------------------
// Name: class TimeData
// Desc: stores timing information for current time step
//-----------------------------------------------------------------------------
class TimeData {
public:
	TimeData ();

	void Reset (double mjd_ref = 0.0);
	// Reset all sim and sys times to 0. Set time warp to 1
	// Disable fixed step mode

	void SetFixedStep(double step);
	// set a fixed time interval for each time step [s]
	// step=0 disables the fixed step modus

	double FixedStep() const { return (bFixedStep ? fixed_step : 0.0); }

	void BeginStep (double deltat, bool running);
	// advance time by deltat (seconds)

	void EndStep (bool running);
	// copy time data from next step to current step

	double JumpTo (double mjd);
	// jump to a new simulation date. Returns jump distance from current time [s]

	void SetWarp (double warp, double delay = 0.0);
	inline double Warp () const { return TWarp; }
	inline bool WarpChanged () const { return bWarpChanged; }
	inline size_t FrameCount() const { return framecount; }

	double MJD(double simt) const;
	// Convert simulation time to MJD

	inline double FPS() const { return fps; }

	double  SysT0;        // current system time since simulation start [s]
	double  SysT1;        // next frame system time (=SysT0+SysDT)
	double  SysDT;        // current system step interval [s]
	double iSysDT;        // 1/SysDT
	double  SimT0;        // current simulation time since simulation start [s]
	double  SimT1;        // next frame simulation time (=SimT0+SimDT)
	double  SimDT;        // current simulation step interval [s] (updated at the beginning of the update phase)
	double  SimDT0;       // time step between currently published state (s0) and the previous state (updated at the end of the update phase)
	double iSimDT;        // 1/SimDT
	double iSimDT0;       // 1/SimDT0
	double  MJD0;         // Modified Julian date at current frame [days]
	double  MJD1;         // Modified Julian date at next frame [days]
	double  MJD_ref;      // Modified Julian date at simulation start [days]

private:
	double  SimT1_ofs, SimT1_inc;  // offset and increment parts of SimT1
	double  fixed_step;   // fixed base time step length (0=variable)
	double  TWarp;        // time acceleration factor
	double  TWarpTarget;  // target acceleration factor
	double  TWarpDelay;   // warp acceleration delay
	bool    bWarpChanged; // time acceleration changed in last step?
	bool    bFixedStep;   // use fixed time steps?
	size_t  framecount;   // number of frames since simulation start (including pause)
	size_t  frame_tick;   // number of frames since last fps calculation
	size_t  sys_tick;     // flush index for fps calculation
	double  syst_acc;     // accumulated system time for fps calculation
	double  fps;          // current frame rate [Hz]
};

struct TimeJumpData {
	double dt;
	int mode;
};

#endif
