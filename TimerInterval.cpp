///////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Interval Measurement Tool
// Copyright (C) 2014-2014 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#include <stdio.h>
#include <stdint.h>
#include <cstdlib>
#include <deque>
#include <vector>
#include <algorithm>

static const size_t WINDOW_SIZE = 11;
static const double PI = 3.1415926535897932384626433832795028842;
static const char *const SPINNER = "/-\\|";

///////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////

static volatile bool g_bRunning = true;

static inline int64_t getFrequency(void)
{
	LARGE_INTEGER value;
	if(QueryPerformanceFrequency(&value))
	{
		return value.QuadPart;
	}
	return -1;
}

static inline int64_t getTimerValue(void)
{
	LARGE_INTEGER value;
	if(QueryPerformanceCounter(&value))
	{
		return value.QuadPart;
	}
	return -1;
}

static inline int64_t doMeasure(void)
{
	std::vector<int64_t>delay(5);

	for(size_t i = 0; i < delay.size(); i++)
	{
		const int64_t timeBeg = getTimerValue();
		Sleep(1);
		const int64_t timeEnd = getTimerValue();
		delay[i] = (timeEnd - timeBeg);
	}

	std::sort(delay.begin(), delay.end());
	return delay[delay.size() / 2];
}

static std::vector<double> init_weights(const uint32_t &filterSize)
{
	if((filterSize < 1) || ((filterSize % 2) != 1))
	{
		throw std::runtime_error("Filter size must be a positive and odd value!");
	}
	
	const double sigma = (((double(filterSize) / 2.0) - 1.0) / 3.0) + (1.0 / 3.0);

	std::vector<double> weights(filterSize);
	double totalWeight = 0.0;

	const uint32_t offset = filterSize / 2;
	const double c1 = 1.0 / (sigma * sqrt(2.0 * PI));
	const double c2 = 2.0 * pow(sigma, 2.0);

	for(uint32_t i = 0; i < filterSize; i++)
	{
		const int32_t x = int32_t(i) - int32_t(offset);
		weights[i] = c1 * exp(-(pow(x, 2.0) / c2));
		totalWeight += weights[i];
	}

	const double adjust = 1.0 / totalWeight;
	for(uint32_t i = 0; i < filterSize; i++)
	{
		weights[i] *= adjust;
	}

	return std::move(weights);
}

static inline double getResult(const std::deque<int64_t> &results, const int64_t &frequency, const std::vector<double> &weights)
{
	size_t k = 0;
	double total = 0.0;

	for(std::deque<int64_t>::const_iterator iter = results.begin(); iter != results.end(); iter++)
	{
		total += (double(*iter) * weights[k++]);
	}

	return (total / double(frequency)) * 1000.0;
}

static BOOL WINAPI ctrl_handler(DWORD CtrlType)
{
	g_bRunning = false;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Main Function
///////////////////////////////////////////////////////////////////////////////

static int tiimt_main(int argc, char* argv[])
{
	printf("Timer Interrupt Interval Measurement Tool [%s]\n\n", __DATE__);
	printf("Initializing...");

	const int64_t frequency = getFrequency();
	const std::vector<double> weights = init_weights(WINDOW_SIZE);
	size_t spinner = 0;
	std::deque<int64_t> results;

	while(g_bRunning)
	{
		if(!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
		{
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		}

		while(results.size() >= WINDOW_SIZE)
		{
			results.pop_front();
		}

		results.push_back(doMeasure());

		if(results.size() == WINDOW_SIZE)
		{
			const double result = getResult(results, frequency, weights);
			printf("\rCurrent Timer Interval: %4.1f ms [%c]", result, SPINNER[spinner]);
			spinner = (spinner + 1) % 4;
		}
	}

	printf("\n\nCTRL+C received: Application will exit now...\n\n");
	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Entry Point
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	int ret = -1;

#ifndef _DEBUG
	__try
	{
		SetConsoleCtrlHandler(ctrl_handler, TRUE);
		ret = tiimt_main(argc, argv);
	}
	__except(1)
	{
		FatalAppExitA(0, "GURU MEDITATION: Unhandeled Exception Error!");
		for(;;) _exit(666);
	}
#else
	ret = tiimt_main(argc, argv);
#endif

	return ret;
}
