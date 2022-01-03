#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include "dae.h"

// stub for isisicpint library. We used to use ISISICPINT to provide an ISISICP built into ISISDAE for testing without DCOM
// but that needs extra DLL libraries from isisicp copying over which can lead to DLL conflicts
  
int	ISISICPINT::updateStatusXML2(const std::string& status_xml_in, std::string& status_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::beginRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::endRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::pauseRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::resumeRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::saveRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::abortRun(std::string& messages)
{
    return 0;
}

int ISISICPINT::startSEWait(std::string& messages)
{
    return 0;
}

int ISISICPINT::endSEWait(std::string& messages)
{
    return 0;
}

int ISISICPINT::getSpectrum(long spectrum_number, long period, std::vector<double>& time_channels, std::vector<double>& signal, bool as_histogram, bool as_distribution, long& sum, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeTCB(const std::string& tcb_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeDAEsettings(const std::string& dae_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::getSpectrumIntegral(long spectrum_number, long period, float time_low, float time_high, long& counts, std::string& messages)
{
    return 0;
}

int	ISISICPINT::getSpectraIntegral(const std::vector<long>& spectrum_numbers, long period, const std::vector<float>& time_low, const std::vector<float>& time_high, std::vector<long>& counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::getEventSpectraIntegral(long spec_start, long nspectra, long period, std::vector<long>& counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::getSpectraIntegral2(long spec_start, long nspectra, long period, float time_low, float time_high, std::vector<long>& counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::rio(long address, long& value, std::string& messages)
{
    return 0;
}

long ISISICPINT::getRawFramesTotal(std::string& messages)
{
    return 0;
}

long ISISICPINT::getGoodFramesTotal(std::string& messages)
{
    return 0;
}

long ISISICPINT::getSpectrumNumberForMonitor(long mon_num, std::string& messages)
{
    return 0;
}

double ISISICPINT::getGoodUAmpH(std::string& messages)
{
    return 0;
}

int ISISICPINT::getSpectraSum(long period, long first_spec, long num_spec, long spec_type, double time_low, double time_high, std::vector<long>& sums, std::vector<long>& max_vals, std::vector<long>& spec_nums, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeSample(const std::string& sample_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeUser(const std::string& user_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeHardwarePeriods(const std::string& periods_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::changePeriod(LONG period_number, std::string& messages)
{
    return 0;
}

int ISISICPINT::dumpDebugInfo(std::string& messages)
{
    return 0;
}

int ISISICPINT::VMEWriteValue(ULONG card_id, ULONG card_address, ULONG word_size, ULONG value, ULONG mode, std::string& messages)
{
    return 0;
}

int ISISICPINT::VMEReadValue(ULONG card_id, ULONG card_address, ULONG word_size, ULONG* value, std::string& messages)
{
    return 0;
}

int ISISICPINT::sumAllSpectra(long& counts, long& bin0_counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::sumAllHistogramMemory(long& counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::VMEReadArray(ULONG card_id, ULONG card_address, std::vector<long>& values, ULONG num_values, std::string& messages)
{
    return 0;
}

int ISISICPINT::VMEWriteArray(ULONG card_id, ULONG card_address, const std::vector<long>& values, std::string& messages)
{
    return 0;
}

int ISISICPINT::VMEReadValuesToString(ULONG card_id, ULONG card_address, ULONG word_size, ULONG num_values, std::string& values, std::string& messages)
{
    return 0;
}

int ISISICPINT::QXReadArray(ULONG card_id, ULONG card_address, std::vector<long>& values, ULONG num_values, ULONG trans_type, std::string& messages)
{
    return 0;
}

int ISISICPINT::QXWriteArray(ULONG card_id, ULONG card_address, const std::vector<long>& values, ULONG trans_type, std::string& messages)
{
    return 0;
}

int ISISICPINT::refreshCachedValues(std::string& messages)
{
    return 0;
}

int ISISICPINT::setOptions(const std::string& options_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::getOptions(std::string& options_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::updateCRPT(std::string& messages)
{
    return 0;
}

int ISISICPINT::storeCRPT(std::string& messages)
{
    return 0;
}

int ISISICPINT::setICPValueLong(const std::string& name, LONG value, std::string& messages)
{
    return 0;
}

int ISISICPINT::notifyICP(LONG event_id, const std::string& param, std::string& messages)
{
    return 0;
}

int ISISICPINT::setBlocksTable(const string_table_t& table, std::string& messages)
{
    return 0;
}

int ISISICPINT::setSampleParameters(const string_table_t& table, std::string& messages)
{
    return 0;
}

int ISISICPINT::setBeamlineParameters(const string_table_t& table, std::string& messages)
{
    return 0;
}

int ISISICPINT::setUserParameters(long rbno, const string_table_t& table, std::string& messages)
{
    return 0;
}

int ISISICPINT::areYouThere(void)
{
    return 0;
}

int ISISICPINT::changeUpdateSettings(const std::string& update_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::getNPRatio(float& current, float& average, std::string& messages)
{
    return 0;
}

long ISISICPINT::getTotalCounts(std::string& messages)
{
    return 0;
}

double ISISICPINT::getMEvents(std::string& messages)
{
    return 0;
}

double ISISICPINT::getMEventsPeriod(long period, std::string& messages)
{
    return 0;
}

int ISISICPINT::getCurrentPeriodNumber(long& period, long& daq_period, std::string& messages)
{
    return 0;
}

int ISISICPINT::snapshotCRPT(const std::string& filename, bool do_update, bool do_pause, std::string& messages)
{
    return 0;
}

int ISISICPINT::changePeriodWhileRunning(LONG period, bool pause_first, std::string& messages)
{
    return 0;
}

int ISISICPINT::getDAEsettings(const std::string& dae_xml_in, std::string& dae_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::getHardwarePeriods(const std::string& periods_xml_in, std::string& periods_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::getTCB(const std::string& tcb_xml_in, std::string& tcb_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::getUpdateSettings(const std::string& update_xml_in, std::string& update_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::changeNumberOfSoftwarePeriods(LONG nperiod, std::string& messages)
{
    return 0;
}

long ISISICPINT::getRunState(std::string& messages)
{
    return 0;
}

int ISISICPINT::beginRunEx(LONG options, LONG period, std::string& messages)
{
    return 0;
}

int ISISICPINT::fillWithTestPattern(ULONG pattern, std::string& messages)
{
    return 0;
}

int ISISICPINT::getVetoStatus(std::string& veto_status, std::string& messages)
{
    return 0;
}

int ISISICPINT::loadDAEWithData(const std::string& file_name, LONG options, std::string& messages)
{
    return 0;
}

int ISISICPINT::setVeto(const std::string& name, bool enable, std::string& messages)
{
    return 0;
}

int ISISICPINT::checkTestPattern(ULONG pattern, std::string& messages)
{
    return 0;
}

long ISISICPINT::getRunNumber(std::string& messages)
{
    return 0;
}

int ISISICPINT::changeMonitoringSettings(const std::string& monitor_xml, std::string& messages)
{
    return 0;
}

int ISISICPINT::getMonitoringSettings(const std::string& monitor_xml_in, std::string& monitor_xml_out, std::string& messages)
{
    return 0;
}

int ISISICPINT::getStatusMessages(LONG stream, std::list<std::string>& messages)
{
    return 0;
}

std::string ISISICPINT::getValue(const std::string& name, std::string& messages)
{
    return "";
}

int ISISICPINT::setICPValue(const std::string& name, const std::string& value, std::string& messages)
{
    return 0;
}

long ISISICPINT::getGoodFramesPeriod(std::string& messages)
{
    return 0;
}

double ISISICPINT::getGoodUAmpHPeriod(std::string& messages)
{
    return 0.0;
}

int ISISICPINT::getFramesAllPeriods(std::vector<long>& good_frames, std::vector<long>& raw_frames, std::string& messages)
{
    return 0;
}

int ISISICPINT::getUAmpHAllPeriods(std::vector<float>& good_uamph, std::vector<float>& raw_uamph, std::string& messages)
{
    return 0;
}

int ISISICPINT::updateCRPTSpectra(LONG period, LONG spec_start, LONG nspec, std::string& messages)
{
    return 0;
}

int ISISICPINT::getCRPTSpectraIntegral(const std::vector<long>& spectrum_numbers, long period, const std::vector<float>& time_low, const std::vector<float>& time_high, std::vector<long>& counts, std::string& messages)
{
    return 0;
}

int ISISICPINT::quit(std::string& messages)
{
    return 0;
}

int ISISICPINT::requestEndRunAfterNextSequenceCompletes(std::string& messages)
{
    return 0;
}

bool ISISICPINT::isFinalSequenceComplete(std::string& messages)
{
    return 0;
}

int ISISICPINT::getSpectrumNumbersForTR(long time_regime, long& spec_min, long& spec_max, std::string& messages)
{
    return 0;
}

long ISISICPINT::getNumberOfPeriods(std::string& messages)
{
    return 0;
}

// sample environment

int ISISICPINT::SELogValue(long run_number, const std::string& source, const std::string& iso_time, const std::string& block_name, const std::string& block_value)
{
    return 0;
}

int ISISICPINT::SEClose()
{
    return 0;
}

int ISISICPINT::SENewSECIConfig(const std::string& config_name)
{
    return 0;
}

int ISISICPINT::SESetBlockValue(const std::string& block_name, const std::string& block_value)
{
    return 0;
}

int ISISICPINT::SESetBlockDetails(const std::string& block_name, const std::string& setpoint_value, const std::string& vi_name, 
									   const std::string& read_control_label, const std::string& set_control_label, 
									   const std::string& button_control_label, long options, const std::string& nexus_name, 
									   float low_limit, float high_limit, const std::string& units, const std::string& current_value)
{
    return 0;
}

int ISISICPINT::SENewMeasurement(const std::string& label, std::string& id)
{
    return 0;
}

int ISISICPINT::SEGetMeasurementLabel(const std::string& id, std::string& label)
{
    return 0;
}

template <typename T>
int ISISICPINT::SESetBlockValues(const std::vector<std::string>& block_names, const std::vector<T>& block_values)
{
    return 0;
}

template <>
int ISISICPINT::SESetBlockValues(const std::vector<std::string>& block_names, const std::vector<std::string>& block_values)
{
    return 0;
}

template <typename T>
int ISISICPINT::SELogValues(LONG run_number, const std::string& source, const std::vector<std::string>&  iso_times, const std::vector<std::string>&  block_names, const std::vector<T>&  block_values)
{
    return 0;
}

template <>
int ISISICPINT::SELogValues(LONG run_number, const std::string& source, const std::vector<std::string>&  iso_times, const std::vector<std::string>&  block_names, const std::vector<std::string>&  block_values)
{
    return 0;
}

int ISISICPINT::SEGetMeasurementID(const std::string& label, std::string& measurement_id)
{
    return 0;
}

int ISISICPINT::SESetMeasurementLabel(const std::string& measurement_id, const std::string& label)
{
    return 0;
}

int ISISICPINT::SESetBlockSetpoint(const std::string& block_name, const std::string& block_setpoint)
{
    return 0;
}

int ISISICPINT::SESetBlockSetpoints(const std::vector<std::string>& block_names, const std::vector<std::string>& block_setpoints)
{
    return 0;
}

std::string ISISICPINT::SEGetSECIConfig()
{
    return "";
}

int ISISICPINT::SELogValuesAsync(LONG run_number, const std::string& source, const std::vector<std::string>& iso_times, const std::vector<std::string>& block_names, const std::vector<std::string>& block_values)
{
    return 0;
}

template <typename T>
int ISISICPINT::SEGetValues(long run_number, const std::string& source, const std::string& block_name, std::vector<std::string>& iso_times, std::vector<T>& block_values)
{
    return 0;
}

template <>
int ISISICPINT::SEGetValues(long run_number, const std::string& source, const std::string& block_name, std::vector<std::string>& iso_times, std::vector<std::string>& block_values)
{
    return 0;
}

int ISISICPINT::SEClearLoggedValues(long run_number)
{
    return 0;
}

std::string ISISICPINT::SEExecSQL(const std::string& sql)
{
    return "";
}

template int ISISICPINT::SEGetValues(long run_number, const std::string& source, const std::string& block_name, std::vector<std::string>& iso_times, std::vector<float>& block_values);
template int ISISICPINT::SESetBlockValues(const std::vector<std::string>& block_names, const std::vector<float>& block_values);
template int ISISICPINT::SELogValues(long run_number, const std::string& source, const std::vector<std::string>& iso_times, const std::vector<std::string>& block_names, const std::vector<float>& block_values);
