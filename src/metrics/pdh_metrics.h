#pragma once
#include <vector>
#include <string>

bool PdhInit();
void PdhShutdown();

std::vector<double> PdhSamplePerCoreCpu();

std::wstring PdhSampleDiskLine();
std::wstring PdhSampleNetLine();

bool PdhSampleDiskTotals(double &readBps, double &writeBps);
bool PdhSampleNetTotals(double &sentBps, double &recvBps);
