#define __BTSTACK_FILE__ "hid_host_demo.c"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_config.h"
#include "btstack.h"
//#include "../../esp-idf/components/freertos/include/freertos/FreeRTOS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300


//SPI PINS
#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   22


spi_device_handle_t spi;


// SDP
static uint8_t            hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t           hid_descriptor_len;

static uint16_t           hid_control_psm;
static uint16_t           hid_interrupt_psm;

static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;

// L2CAP
static uint16_t           l2cap_hid_control_cid;
static uint16_t           l2cap_hid_interrupt_cid;

// MBP 2016
static const char * remote_addr_string = "0C:FC:83:51:B3:F5";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;


/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP is initialized 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Initialize L2CAP 
    l2cap_init();

    // Disable stdout buffering
    setbuf(stdout, NULL);
}
/* LISTING_END */

// Send data by SPI
void send_data_spi(const uint8_t *data, int len)
{
	esp_err_t ret;
	spi_transaction_t t;
	if (len==0) return;
	memset(&t, 0, sizeof(t));
	t.length = len*8; // len is in bytes, transation length is in bits
	t.tx_buffer = data;
	t.user = (void*)1;
	ret = spi_device_transmit(spi, &t);
	assert(ret==ESP_OK);
}



/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        des_iterator_next(&prot_it);
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_control_psm);
                                        printf("HID Control PSM: 0x%04x\n", (int) hid_control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_interrupt_psm);
                                            printf("HID Interrupt PSM: 0x%04x\n", (int) hid_interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING) continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    const uint8_t * descriptor = de_get_string(element);
                                    hid_descriptor_len = de_get_data_size(element);
                                    memcpy(hid_descriptor, descriptor, hid_descriptor_len);
                                    printf("HID Descriptor:\n");
                                    printf_hexdump(hid_descriptor, hid_descriptor_len);
                                }
                            }                        
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            if (!hid_control_psm) {
                printf("HID Control PSM missing\n");
                break;
            }
            if (!hid_interrupt_psm) {
                printf("HID Interrupt PSM missing\n");
                break;
            }
            printf("Setup HID\n");
            status = l2cap_create_channel(packet_handler, remote_addr, hid_control_psm, 48, &l2cap_hid_control_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;
    }
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HCI Events.
 */

btstack_hid_parser_t parser;
int32_t  value;
uint16_t usage;
uint16_t usage_page;
uint8_t spi_data[3];

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    /* LISTING_PAUSE */
    uint8_t   event;
    bd_addr_t event_addr;
    uint8_t   status;
    uint16_t  l2cap_cid;

    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("Start SDP HID query for remote HID Device.\n");
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
                    }
                    break;

                /* LISTING_PAUSE */
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                /* LISTING_RESUME */

                case L2CAP_EVENT_CHANNEL_OPENED: 
                    status = packet[2];
                    if (status){
                        printf("L2CAP Connection failed: 0x%02x\n", status);
                        break;
                    }
                    l2cap_cid  = little_endian_read_16(packet, 13);
                    if (!l2cap_cid) break;
                    if (l2cap_cid == l2cap_hid_control_cid){
                        status = l2cap_create_channel(packet_handler, remote_addr, hid_interrupt_psm, 48, &l2cap_hid_interrupt_cid);
                        if (status){
                            printf("Connecting to HID Control failed: 0x%02x\n", status);
                            break;
                        }
                    }                        
                    if (l2cap_cid == l2cap_hid_interrupt_cid){
                        printf("HID Connection established\n");
                    }
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            // for now, just dump incoming data
            if (channel == l2cap_hid_interrupt_cid){
                //hid_host_handle_interrupt_report(packet,  size);
    		// check if HID Input Report
    		if (size < 1) return;
    		if (*packet != 0xa1) return; 
    		packet++;
    		size--;
		portDISABLE_INTERRUPTS();
    		btstack_hid_parser_init(&parser, hid_descriptor, hid_descriptor_len, BTSTACK_HID_REPORT_TYPE_INPUT, packet, size);
    		while (btstack_hid_parser_has_more(&parser)){
        	    btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);
		    //printf("usage_page: %x, usage: %x, value: %x\n", usage_page, usage, value);
		    if(usage_page == 0xc && value == 1)
		        switch(usage)    //void send_data_spi(const uint8_t *data, int len)
			{
			    case 0xe9:
				//puts("Button C\n");
				spi_data[0] = 3;
				break;
			    case 0x46:
				//puts("Button B or down\n");
				spi_data[0] = 2;
				break;
			    case 0xea:
			        //puts("Button D\n");
				spi_data[0] = 4;
				break;
			    default:
				break;
			}
		    else if(usage_page == 0x9 && value == 1)
			//puts("Button A or up\n");
		    	spi_data[0] = 1;
		    else if(usage_page == 0x1)
			switch(usage)
			{
			    case 0x30:
			        //printf("Joystick left/right:%d\n", value);
			       	spi_data[1] = (uint8_t)(value+15);
				spi_data[0] = 5;
				break;
			    case 0x31:
			        //printf("Joystick up/down:%d\n", value);
				spi_data[2] = (uint8_t)(value+15);
				spi_data[0] = 6;
				break;
			    default:
				break;
			}
		    send_data_spi(spi_data, 3);
    		}
		portENABLE_INTERRUPTS();
            } else if (channel == l2cap_hid_control_cid){
                printf("HID Control: ");
                printf_hexdump(packet, size);
            } else {
                break;
            }
        default:
            break;
    }
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    esp_err_t ret;
    spi_bus_config_t buscfg={
	    .miso_io_num=PIN_NUM_MISO,
	    .mosi_io_num=PIN_NUM_MOSI,
	    .sclk_io_num=PIN_NUM_CLK,
	    .quadwp_io_num=-1,
	    .quadhd_io_num=-1,
	    .max_transfer_sz=20
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=10*1000*1000,
		.mode=0,
		.spics_io_num=PIN_NUM_CS,
		.queue_size=7
	};
	ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	ESP_ERROR_CHECK(ret);
	ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);

    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    printf("Initialisation OK\n");
    return 0;
}

/* EXAMPLE_END */
