/***********************************************************************
 * rxnode.cpp          
 * 
 * This source file implements a two channel receiver on the USRP E310.
 *  
 * VERSION 10.15.15:1 -- initial version by M.Overdick
 * 
 **********************************************************************/

#include "includes.hpp"

    // tweakable parameters
#define DURATION        5           // Length of time to record in seconds
#define SAMPRATE        100e3       // sampling rate (Hz)
#define CARRIERFREQ     100.0e6     // carrier frequency (Hz)
#define CLOCKRATE       30.0e6      // clock rate (Hz) 
#define RXGAIN          0.0         // Rx frontend gain in dB
#define SPB             1000        // samples per buffer

typedef boost::function<uhd::sensor_value_t (const std::string&)> get_sensor_fn_t;

bool check_locked_sensor(std::vector<std::string> sensor_names, const char* sensor_name, get_sensor_fn_t get_sensor_fn, double setup_time);

/***********************************************************************
 * Signal handlers
 **********************************************************************/
static bool stop_signal_called = false;
void sig_int_handler(int){stop_signal_called = true;}

/***********************************************************************
 * Main function
 **********************************************************************/
int UHD_SAFE_MAIN(int argc, char *argv[]){
    uhd::set_thread_priority_safe();

    /** Constant Decalartions *****************************************/
    const INT32U time = DURATION*(SAMPRATE/SPB);
    
    /** Variable Declarations *****************************************/
    
    std::vector< CINT16 >   ch0_rxbuff(SPB);        // receive buffer (Ch 0)
    std::vector< CINT16 >   ch1_rxbuff(SPB);        // receive buffer (Ch 1)
    
    std::vector< CINT16 >   ch0_out(time*SPB);  // Output buffer (Ch 0)
    std::vector< CINT16 >   ch1_out(time*SPB);  // Output buffer (Ch 1)
    
    std::vector< CINT16 *>  rxbuffs(2);             // Vector of pointers to rxbuffs
    
        // Holds the number of received samples returned by rx_stream->recv()
    INT16U num_rx_samps;

        // Counters
    INT16U i,j,k;                       // Generic counters
    INT32U rx_ctr;                      // Counts loops through main while()
    
    
    /** Variable Initializations **************************************/
    rxbuffs[0] = &ch0_rxbuff.front();
    rxbuffs[1] = &ch1_rxbuff.front();
    
    /** Main code *****************************************************/

        // set USRP Rx params
    uhd::usrp::multi_usrp::sptr usrp_rx = uhd::usrp::multi_usrp::make(std::string("")); // create a usrp device
    uhd::tune_request_t tune_request(CARRIERFREQ);                                      // validate tune request
    usrp_rx->set_master_clock_rate(CLOCKRATE);                                          // set clock rate
    usrp_rx->set_clock_source(std::string("internal"));                                 // lock mboard clocks
    usrp_rx->set_rx_subdev_spec(std::string("A:A A:B"));                                // select the subdevice
    usrp_rx->set_rx_rate(SAMPRATE,0);                                                   // set the sample rate (Ch 0)
    usrp_rx->set_rx_rate(SAMPRATE,1);                                                   // set the sample rate (Ch 1)
    usrp_rx->set_rx_freq(tune_request,0);                                               // set the center frequency (Ch 0)
    usrp_rx->set_rx_freq(tune_request,1);                                               // set the center frequency (Ch 1)
    usrp_rx->set_rx_gain(RXGAIN,0);                                                     // set the rf gain (Ch 0)
    usrp_rx->set_rx_gain(RXGAIN,1);                                                     // set the rf gain (Ch 1)
    usrp_rx->set_rx_antenna(std::string("RX2"),0);                                      // set the antenna (Ch 0)
    usrp_rx->set_rx_antenna(std::string("RX2"),1);                                      // set the antenna (Ch 1)
    boost::this_thread::sleep(boost::posix_time::seconds(1.0));                         // allow for some setup time

        // check Ref and LO Lock detect for Rx
    check_locked_sensor(usrp_rx->get_rx_sensor_names(0), "lo_locked", boost::bind(&uhd::usrp::multi_usrp::get_rx_sensor, usrp_rx, _1, 0), 1.0); 

        // create a receive streamer
    uhd::stream_args_t stream_args_rx("sc16", "sc16");
     stream_args_rx.channels = boost::assign::list_of(0)(1);
    uhd::rx_streamer::sptr rx_stream = usrp_rx->get_rx_stream(stream_args_rx);
    uhd::rx_metadata_t md_rx;

        // report stuff to user (things which may differ from what was requested)
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp_rx->get_rx_rate()/1e6) << std::endl; 
    
        // set sigint so user can terminate via Ctrl-C
    std::signal(SIGINT, &sig_int_handler);
    std::cout << "Press Ctrl + C to stop streaming..." << std::endl; 

        // setup receive streaming
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = false;
    stream_cmd.time_spec = uhd::time_spec_t(0.25)+usrp_rx->get_time_now();  // tell USRP to start streaming 0.25 seconds in the future
    rx_stream->issue_stream_cmd(stream_cmd);

        // grab initial block of received samples from USRP with nice long timeout (gets discarded)
    num_rx_samps = rx_stream->recv(rxbuffs, SPB, md_rx, 3.0);

    while(not stop_signal_called){
            // grab block of received samples from USRP
        num_rx_samps = rx_stream->recv(rxbuffs, SPB, md_rx); 
            // Copy Rx buffers
        for(k = 0; k < SPB; k++){
            ch0_out[(SPB*rx_ctr)+k] = rxbuffs[0][k];
            ch1_out[(SPB*rx_ctr)+k] = rxbuffs[1][k];
        }
        
            // Increment counter
        rx_ctr++;
        
            // Check if full time has passed
        if(rx_ctr == time){
            break;
        }else{}
        
    }   /** while(not stop_signal_called) *****************************/

        // Write buffers to file
    std::cout << "Writing buffers to file..." << std::endl;
    
    std::cout << "    Channel 0..." << std::flush;
    writebuff_CINT16("./RX_Ch0.dat", &ch0_out.front(), time*SPB);
    std::cout << "done!" << std::endl;
    
    std::cout << "    Channel 1..." << std::flush;
    writebuff_CINT16("./RX_Ch1.dat", &ch1_out.front(), time*SPB);
    std::cout << "done!" << std::endl;

    
    return EXIT_SUCCESS;
}   /** main() ********************************************************/

bool check_locked_sensor(std::vector<std::string> sensor_names, const char* sensor_name, get_sensor_fn_t get_sensor_fn, FP64 setup_time){
    if (std::find(sensor_names.begin(), sensor_names.end(), sensor_name) == sensor_names.end())
        return false;

    boost::system_time start = boost::get_system_time();
    boost::system_time first_lock_time;

    std::cout << boost::format("Waiting for \"%s\": ") % sensor_name;
    std::cout.flush();

    while (true) {
        if ((not first_lock_time.is_not_a_date_time()) and
                (boost::get_system_time() > (first_lock_time + boost::posix_time::seconds(setup_time))))
        {
            std::cout << " locked." << std::endl;
            break;
        }
        if (get_sensor_fn(sensor_name).to_bool()){
            if (first_lock_time.is_not_a_date_time())
                first_lock_time = boost::get_system_time();
            std::cout << "+";
            std::cout.flush();
        }
        else {
            first_lock_time = boost::system_time();	// reset to 'not a date time'

            if (boost::get_system_time() > (start + boost::posix_time::seconds(setup_time))){
                std::cout << std::endl;
                throw std::runtime_error(str(boost::format("timed out waiting for consecutive locks on sensor \"%s\"") % sensor_name));
            }
            std::cout << "_";
            std::cout.flush();
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    std::cout << std::endl;
    return true;
}
