# This class contains default value for tcl

Phy/WirelessPhy/OFDMA set g_ 0              ;# cyclic prefix

Mac/802_16 set queue_length_         50     ;# maximum number of packets

Mac/802_16 set frame_duration_       0.005  ;# frame duration
Mac/802_16 set channel_              0      ;# default channel assignment
Mac/802_16 set fbandwidth_           10e+6  ;# frequency bandwidth (Hz)
Mac/802_16 set dl_permutation_       0      ;# downlink permutation PUSC=0, FUSC=1, AMC=2, OFUSC=4 
Mac/802_16 set ul_permutation_       0      ;# uplink permutation PUSC=0, AMC=2, OPUSC=3 
Mac/802_16 set rtg_                  10     ;# number of PS to switch from receiving to transmitting state
Mac/802_16 set ttg_                  10     ;# number of PS to switch from transmitting to receiving state
Mac/802_16 set dcd_interval_         5      ;# interval between the broadcast of DCD messages (max 10s)
Mac/802_16 set ucd_interval_         5      ;# interval between the broadcast of UCD messages (max 10s)
Mac/802_16 set init_rng_interval_    1      ;# time between initial ranging regions assigned by the BS (max 2s). Not used
Mac/802_16 set lost_dlmap_interval_  0.6    ;# timeout value for receiving DL_MAP message (s)
Mac/802_16 set lost_ulmap_interval_  0.6    ;# timeout value for receiving UL_MAP message (s)
Mac/802_16 set t1_timeout_           [expr 5* [Mac/802_16 set dcd_interval_]] ;# wait for DCD timeout
Mac/802_16 set t2_timeout_           [expr 5* [Mac/802_16 set init_rng_interval_]] ;# wait for broadcast ranging timeout
Mac/802_16 set t3_timeout_           0.2    ;# ranging response timeout
Mac/802_16 set t6_timeout_           3      ;# registration response timeout
Mac/802_16 set t12_timeout_          [expr 5* [Mac/802_16 set ucd_interval_]] ;# UCD descriptor timeout
Mac/802_16 set t16_timeout_          0.1    ;# bandwidth request timeout (qos dependant) 
Mac/802_16 set t17_timeout_          5      ;# authentication. Not used
Mac/802_16 set t21_timeout_          0.02   ;# wait for DL_MAP timeout. Replace with 20ms to emulate preamble scanning on channel
Mac/802_16 set contention_rng_retry_ 16     ;# number of retries on ranging requests (contention mode)
Mac/802_16 set init_contention_size_      5 ;# number of opportunities per frame (deprecated by CDMA ranging code)
Mac/802_16 set bw_req_contention_size_    5 ;# number of bw request opportunites (deprecated by CDMA ranging code)
Mac/802_16 set invited_rng_retry_    16     ;# number of retries on ranging requests (invited mode)
Mac/802_16 set request_retry_        2      ;# number of retries on bandwidth allocation requests
Mac/802_16 set reg_req_retry_        3      ;# number of retries on registration requests
Mac/802_16 set tproc_                0.001  ;# time between arrival of last bit of a UL_MAP and effectiveness. Not used
Mac/802_16 set dsx_req_retry_        3      ;# number of retries on DSx requests
Mac/802_16 set dsx_rsp_retry_        3      ;# number of retries on DSx responses
Mac/802_16 set cdma_code_bw_start_   		0      ;# cdma code for bw request (start)
Mac/802_16 set cdma_code_bw_stop_   		63      ;# cdma code for bw request (stop)
Mac/802_16 set cdma_code_init_start_   		64      ;# cdma code for initial request (start)
Mac/802_16 set cdma_code_init_stop_   		127      ;# cdma code for initial request (stop)
Mac/802_16 set cdma_code_cqich_start_   	128     ;# cdma code for cqich request (start)
Mac/802_16 set cdma_code_cqich_stop_   		195      ;# cdma code for cqich request (stop)
Mac/802_16 set cdma_code_handover_start_	196     ;# cdma code for handover request (start)
Mac/802_16 set cdma_code_handover_stop_ 	255      ;# cdma code for handover request (stop)

Mac/802_16 set rng_backoff_start_    2      ;# initial backoff window size for ranging requests
Mac/802_16 set rng_backoff_stop_     6      ;# maximal backoff window size for ranging requests
Mac/802_16 set bw_backoff_start_     2      ;# initial backoff window size for bandwidth requests
Mac/802_16 set bw_backoff_stop_      6      ;# maximal backoff window size for bandwitdh requests

Mac/802_16 set scan_duration_        50     ;# duration (in frames) of scan interval
Mac/802_16 set interleaving_interval_ 50    ;# duration (in frames) of interleaving interval
Mac/802_16 set scan_iteration_       2      ;# number of scan iterations
Mac/802_16 set t44_timeout_          0.1    ;# timeout value for scan requests (s)
Mac/802_16 set max_dir_scan_time_    0.2    ;#max scan for each neighbor BSs (s)
Mac/802_16 set client_timeout_       0.5    ;# timeout value for detecting out of range client
Mac/802_16 set nbr_adv_interval_     0.5    ;# interval between 2 MOB-NBR_ADV messages (s)
Mac/802_16 set scan_req_retry_       3

Mac/802_16 set lgd_factor_           1      ;# coefficient used to trigger Link Going Down (1 for no trigger)
Mac/802_16 set print_stats_          0      ;# true to activate print of statistics
Mac/802_16 set rxp_avg_alpha_        1      ;# history coefficient for statistic on receiving power [avg=alpha*new_stat+(1-alpha)*avg]
Mac/802_16 set delay_avg_alpha_      1      ;# coefficient for statistic on frame delay
Mac/802_16 set jitter_avg_alpha_     1      ;# coefficient for statistic on frame jitter
Mac/802_16 set loss_avg_alpha_       1      ;# coefficient for statistic on frame loss
Mac/802_16 set throughput_avg_alpha_ 1      ;# coefficient for statistic on throughput
Mac/802_16 set throughput_delay_     0.02   ;# interval time to update throughput when there is no traffic

Mac/802_16 set data_loss_rate_        -1    ;# to simulate uniform packet loss
Mac/802_16 set arqfb_in_dl_data_     0
Mac/802_16 set arqfb_in_ul_data_     0
Mac/802_16 set arq_block_size_       128
Mac/802_16 set disable_interference_ 0      ;# enable/disable interference/BLER computation

Agent/WimaxCtrl set debug_ 0                ;# display information about WiMAX controller used to handle scanning
Agent/WimaxCtrl set adv_interval_ 1.0       ;# interval between the exchange of information between WiMAX controller
Agent/WimaxCtrl set default_association_level_ 0 ;# assiciation level handled by the WiMAX controller
Agent/WimaxCtrl set synch_frame_delay_ 50   ;# delay used to synchronize with neighbors before sending a scan response to MS

Mac/802_16/BS set dlratio_ 0.3              ;# portion of the frame dedicated to downlink
Mac/802_16/SS set dlratio_ 0.3              ;# portion of the frame dedicated to downlink

Mac/802_16/BS set Repetition_code_     2    ;# repetition code for DL_MAP
