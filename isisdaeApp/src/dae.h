#ifndef ISISICPINT_H
#define ISISICPINT_H

#ifdef ISISICPINT_EXPORTS
#define ISISICPINT_DllExport __declspec(dllexport)
#else
#define ISISICPINT_DllExport __declspec(dllimport) 
#endif /* ISISICPINT_EXPORTS */

namespace ISISICPINT
{
	typedef std::vector< std::vector<std::string> > string_table_t;
	ISISICPINT_DllExport extern int(updateStatusXML)(std::string& cluster_xml, std::string& messages);
	ISISICPINT_DllExport extern int(beginRun)(std::string& messages);
	ISISICPINT_DllExport extern int(endRun)(std::string& messages);
	ISISICPINT_DllExport extern int(pauseRun)(std::string& messages);
	ISISICPINT_DllExport extern int(resumeRun)(std::string& messages);
	ISISICPINT_DllExport extern int(saveRun)(std::string& messages);
	ISISICPINT_DllExport extern int(abortRun)(std::string& messages);
	ISISICPINT_DllExport extern int(startSEWait)(std::string& messages);
	ISISICPINT_DllExport extern int(endSEWait)(std::string& messages);
	ISISICPINT_DllExport extern int(getSpectrum)(long spectrum_number, long period, std::vector<double>& time_channels, std::vector<double>& signal, bool as_histogram, bool as_distribution, long& sum, std::string& messages);
	ISISICPINT_DllExport extern int(changeTCB)(const std::string& tcb_xml, std::string& messages);
	ISISICPINT_DllExport extern int(changeDAEsettings)(const std::string& dae_xml, std::string& messages);
	ISISICPINT_DllExport extern int(getSpectrumIntegral)(long spectrum_number, long period, float time_low, float time_high, long& counts, std::string& messages);
	ISISICPINT_DllExport extern int(getSpectraIntegral)(const std::vector<long>& spectrum_numbers, long period, const std::vector<float>& time_low, const std::vector<float>& time_high, std::vector<long>& counts, std::string& messages);
	ISISICPINT_DllExport extern int(getsect)(long start, long length, std::vector<long>& values, std::string& messages);
	ISISICPINT_DllExport extern int(rio)(long address, long& value, std::string& messages);
	ISISICPINT_DllExport extern std::string(getValue)(const std::string& name, std::string& messages); // retval
	ISISICPINT_DllExport extern long(getRawFramesTotal)(std::string& messages); // retval
	ISISICPINT_DllExport extern long(getGoodFramesTotal)(std::string& messages); // retval
	ISISICPINT_DllExport extern double(getGoodUAmpH)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(getSpectraSum)(long period, long first_spec, long num_spec, long spec_type, double time_low, double time_high, std::vector<long>& sums, std::vector<long>& max_vals, std::vector<long>& spec_nums, std::string& messages);
	ISISICPINT_DllExport extern int(changeSample)(const std::string& sample_xml, std::string& messages);
	ISISICPINT_DllExport extern int(changeUser)(const std::string& user_xml, std::string& messages);
	ISISICPINT_DllExport extern int(changeHardwarePeriods)(const std::string& periods_xml, std::string& messages);
	ISISICPINT_DllExport extern int(changePeriod)(long period_number, std::string& messages);
	ISISICPINT_DllExport extern int(dumpDebugInfo)(std::string& messages);
	ISISICPINT_DllExport extern int(VMEWriteValue)(ULONG card_id, ULONG card_address, ULONG word_size, ULONG value, ULONG mode, std::string& messages);
	ISISICPINT_DllExport extern int(VMEReadValue)(ULONG card_id, ULONG card_address, ULONG word_size, ULONG* value, std::string& messages);
	ISISICPINT_DllExport extern int(sumAllSpectra)(long& counts, long& bin0_counts, std::string& messages);
	ISISICPINT_DllExport extern int(sumAllHistogramMemory)(long& counts, std::string& messages);
	ISISICPINT_DllExport extern int(VMEReadArray)(ULONG card_id, ULONG card_address, std::vector<long>& values, ULONG num_values, std::string& messages);
	ISISICPINT_DllExport extern int(VMEWriteArray)(ULONG card_id, ULONG card_address, const std::vector<long>& values, std::string& messages);
	ISISICPINT_DllExport extern int(QXReadArray)(ULONG card_id, ULONG card_address, std::vector<long>& values, ULONG num_values,  ULONG trans_type, std::string& messages);
	ISISICPINT_DllExport extern int(QXWriteArray)(ULONG card_id, ULONG card_address, const std::vector<long>& values, ULONG trans_type, std::string& messages);
	ISISICPINT_DllExport extern int(VMEReadValuesToString)(ULONG card_id, ULONG card_address, ULONG word_size, ULONG num_values, std::string& values, std::string& messages);
	ISISICPINT_DllExport extern int(refreshCachedValues)(std::string& messages);
	ISISICPINT_DllExport extern int(setOptions)(const std::string& options_xml, std::string& messages);
	ISISICPINT_DllExport extern int(getOptions)(std::string& options_xml, std::string& messages);
	ISISICPINT_DllExport extern std::string(getArrayValue)(const std::string& name, const std::vector<long>& arg, std::string& messages); // retval
	ISISICPINT_DllExport extern int(updateCRPT)(std::string& messages);
	ISISICPINT_DllExport extern int(storeCRPT)(std::string& messages);
	ISISICPINT_DllExport extern int(setICPValueLong)(const std::string& name, long value, std::string& messages);
	ISISICPINT_DllExport extern int(notifyICP)(long event_id, const std::string& param, std::string& messages);
	ISISICPINT_DllExport extern int(setBlocksTable)(const string_table_t& table, std::string& messages);
	ISISICPINT_DllExport extern int(setSampleParameters)(const string_table_t& table, std::string& messages);
	ISISICPINT_DllExport extern int(setBeamlineParameters)(const string_table_t& table, std::string& messages);
	ISISICPINT_DllExport extern int(setUserParameters)(long rbno, const string_table_t& table, std::string& messages);
	ISISICPINT_DllExport extern int(areYouThere)(void);
	ISISICPINT_DllExport extern int(changeUpdateSettings)(const std::string& update_xml, std::string& messages);
	ISISICPINT_DllExport extern int(getNPRatio)(float& current, float& average, std::string& messages);
	ISISICPINT_DllExport extern long(getTotalCounts)(std::string& messages); //retval
	ISISICPINT_DllExport extern double(getMEvents)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(getCurrentPeriodNumber)(long& period, long& daq_period, std::string& messages);
	ISISICPINT_DllExport extern int(snapshotCRPT)(const std::string& filename, bool do_update, bool do_pause, std::string& messages);
	ISISICPINT_DllExport extern int(changePeriodWhileRunning)(long period, bool pause_first, std::string& messages);
	ISISICPINT_DllExport extern int(getDAEsettings)(const std::string& dae_xml_in, std::string& dae_xml_out, std::string& messages);
	ISISICPINT_DllExport extern int(getHardwarePeriods)(const std::string& periods_xml_in, std::string& periods_xml_out, std::string& messages);
	ISISICPINT_DllExport extern int(getTCB)(const std::string& tcb_xml_in, std::string& tcb_xml_out, std::string& messages);
	ISISICPINT_DllExport extern int(getUpdateSettings)(const std::string& update_xml_in, std::string& update_xml_out, std::string& messages);
	ISISICPINT_DllExport extern int(changeNumberOfSoftwarePeriods)(long nperiod, std::string& messages);
	ISISICPINT_DllExport extern long(getRunState)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(beginRunEx)(long options, long period, std::string& messages);
	ISISICPINT_DllExport extern int(fillWithTestPattern)(unsigned long pattern, std::string& messages);
	ISISICPINT_DllExport extern int(getVetoStatus)(std::string& veto_status, std::string& messages);
	ISISICPINT_DllExport extern int(loadDAEWithData)(const std::string& file_name, long options, std::string& messages);
	ISISICPINT_DllExport extern int(setVeto)(const std::string& name, bool enable, std::string& messages);
	ISISICPINT_DllExport extern int(checkTestPattern)(unsigned long pattern, std::string& messages);
	ISISICPINT_DllExport extern long(getRunNumber)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(changeMonitoringSettings)(const std::string& monitor_xml, std::string& messages);
	ISISICPINT_DllExport extern int(getMonitoringSettings)(const std::string& monitor_xml_in, std::string& monitor_xml_out, std::string& messages);
	ISISICPINT_DllExport extern int(getStatusMessages)(long stream, std::list<std::string>& messages);
	ISISICPINT_DllExport extern long(getGoodFramesPeriod)(std::string& messages); // retval
	ISISICPINT_DllExport extern double(getGoodUAmpHPeriod)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(getFramesAllPeriods)(std::vector<long>& good_frames, std::vector<long>& raw_frames, std::string& messages);
	ISISICPINT_DllExport extern int(getUAmpHAllPeriods)(std::vector<float>& good_uamph, std::vector<float>& raw_uamph, std::string& messages);
	ISISICPINT_DllExport extern double(getMEventsPeriod)(long period, std::string& messages); // retval
	ISISICPINT_DllExport extern int(updateCRPTSpectra)(long period, long spec_start, long nspec, std::string& messages);
	ISISICPINT_DllExport extern int(getCRPTSpectraIntegral)(const std::vector<long>& spectrum_numbers, long period, const std::vector<float>& time_low, const std::vector<float>& time_high, std::vector<long>& counts, std::string& messages);
	ISISICPINT_DllExport extern int(quit)(std::string& messages);
	ISISICPINT_DllExport extern int(requestEndRunAfterNextSequenceCompletes)(std::string& messages);
	ISISICPINT_DllExport extern bool(isFinalSequenceComplete)(std::string& messages); //retval
	ISISICPINT_DllExport extern int(getSpectrumNumbersForTR)(long time_regime, long& spec_min, long& spec_max, std::string& messages);
	ISISICPINT_DllExport extern int(getSpectraIntegral2)(long spec_start, long nspectra, long period, float time_low, float time_high, std::vector<long>& counts, std::string& messages);
	ISISICPINT_DllExport extern int(getEventSpectraIntegral)(long spec_start, long nspectra, long period, std::vector<long>& counts, std::string& messages);
    ISISICPINT_DllExport extern long(getNumberOfPeriods)(std::string& messages); // retval
	ISISICPINT_DllExport extern int(updateStatusXML2)(const std::string& status_xml_in, std::string& status_xml_out, std::string& messages);

	ISISICPINT_DllExport extern int(SELogValue)(long run_number, const std::string& source, const std::string& iso_time, const std::string& block_name, const std::string& block_value);
	ISISICPINT_DllExport extern int(SEClose)(void);
	ISISICPINT_DllExport extern int(SENewSECIConfig)(const std::string& config_name);
	ISISICPINT_DllExport extern int(SESetBlockValue)(const std::string& block_name, const std::string& block_value);
	ISISICPINT_DllExport extern int(SESetBlockDetails)(const std::string& block_name, const std::string& setpoint_value, const std::string& vi_name, const std::string& read_control_label, const std::string& set_control_label, const std::string& button_control_label, long options, const std::string& nexus_name, float low_limit, float high_limit, const std::string& units, const std::string& current_value);
	ISISICPINT_DllExport extern int(SENewMeasurement)(const std::string& label, std::string& id);
	ISISICPINT_DllExport extern int(SEGetMeasurementLabel)(const std::string& id, std::string& label);
	template <typename T>
	    ISISICPINT_DllExport extern int(SESetBlockValues)(const std::vector<std::string>& block_names, const std::vector<T>& block_values);
	template <typename T>
	    ISISICPINT_DllExport extern int(SELogValues)(long run_number, const std::string& source, const std::vector<std::string>& iso_times, const std::vector<std::string>& block_names, const std::vector<T>& block_values);
	ISISICPINT_DllExport extern int(SEGetMeasurementID)(const std::string& label, std::string& measurement_id);
	ISISICPINT_DllExport extern int(SESetMeasurementLabel)(const std::string& measurement_id, const std::string& label);
	ISISICPINT_DllExport extern int(SESetBlockSetpoint)(const std::string& block_name, const std::string& block_setpoint);
	ISISICPINT_DllExport extern int(SESetBlockSetpoints)(const std::vector<std::string>& block_names, const std::vector<std::string>& block_setpoints);
	ISISICPINT_DllExport extern std::string(SEGetSECIConfig)(); // retval
	ISISICPINT_DllExport extern int(SELogValuesAsync)(long run_number, const std::string& source, const std::vector<std::string>& iso_times, const std::vector<std::string>& block_names, const std::vector<std::string>& block_values);
	template <typename T>
		ISISICPINT_DllExport extern int(SEGetValues)(long run_number, const std::string& source, const std::string& block_name, std::vector<std::string>& iso_times, std::vector<T>& block_values);
	ISISICPINT_DllExport extern int(SEClearLoggedValues)(long run_number);
	ISISICPINT_DllExport extern std::string(SEExecSQL)(const std::string& sql); // retval
};

#endif /* ISISICPINT_H */
