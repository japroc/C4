#include "C4_wpcap.hpp"
#include <iostream>
#include "converter.hpp"

C4_wpcap::C4_wpcap()
: alldevs(0), alldevs_count(0), curr_dev(0)
{}

C4_wpcap::~C4_wpcap()
{}

int C4_wpcap::Get_Int_List()
{
	/* Retrieve the device list on the local machine */
	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs: %s\n", errbuf);
		return 1;
	}

	// Count interfaces
	pcap_if_t *d;
	for (d = alldevs, alldevs_count = 0; d; alldevs_count++, d = d->next);
}

int C4_wpcap::Print_Int_List()
{
	pcap_if_t *d;
	int i = 0;

	/* Print the list */
	for (d = alldevs; d; d = d->next)
	{
		printf("%d. %s", ++i, d->name);
		if (d->description)
			printf(" (%s)\n", d->description);
		else
			printf(" (No description available)\n");
	}

	if (i == 0)
	{
		printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
		return -1;
	}

	return 0;
}

int C4_wpcap::Set_Curr_Dev(std::string name)
{
	pcap_if_t * d;
	unsigned int i;

	for (d = alldevs, i = 0;
		i < alldevs_count;
		d = d->next, i++)
	{
		std::string tmp = d->name;
		if (tmp.find(name) != std::string::npos)
		{
			curr_dev = d;
			return 0;
		}
	}
	
	return -1;
}

pcap_t * C4_wpcap::Listen_arp(std::map<std::string, std::string> & mac_ip)
{
	if (!curr_dev)
	{
		std::cout << "Choose Current Device before" << std::endl;
		return 0;
	}
	pcap_t * adhandle;
	/* Open the device */
	if ((adhandle = pcap_open(curr_dev->name,          // name of the device
		65536,            // portion of the packet to capture. 
		// 65536 guarantees that the whole packet will be captured on all the link layers
		PCAP_OPENFLAG_MAX_RESPONSIVENESS,    // promiscuous mode
		1000,             // read timeout
		NULL,             // authentication on the remote machine
		errbuf            // error buffer
		)) == NULL)
	{
		fprintf(stderr, "\nUnable to open the adapter. %s is not supported by WinPcap\n", curr_dev->name);
		printf("\nUnable to open the adapter. %s\n", pcap_geterr(adhandle));
		return 0;
	}

	//--------------------
	// Create Network mask
	//--------------------

	DWORD netmask;
	if (curr_dev->addresses != NULL)
		/* Retrieve the mask of the first address of the interface */
		netmask = ((struct sockaddr_in *)(curr_dev->addresses->netmask))->sin_addr.S_un.S_addr;
	else
		/* If the interface is without an address we suppose to be in a C class network */
		netmask = 0xffffff;

	netmask = 0xffffff;

	//--------------------
	// Compile filter
	//--------------------

	struct bpf_program fcode;
	if (pcap_compile(adhandle, &fcode, "arp", 1, netmask) < 0)
	{
		fprintf(stderr, "\nUnable to compile the packet filter. Check the syntax.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return 0;
	}

	//--------------------
	// Set filter
	//--------------------

	if (pcap_setfilter(adhandle, &fcode) < 0)
	{
		fprintf(stderr, "\nError setting the filter.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return 0;
	}

	//--------------------
	// Capturing
	//--------------------

	struct pcap_pkthdr *header;
	const u_char *pkt_data;
	int res;
	/* Retrieve the packets */
	while ((res = pcap_next_ex(adhandle, &header, &pkt_data)) >= 0){

		if (res == 0)
			/* Timeout elapsed */
			continue;

		// check for response
		// 1 - request
		// 2 - reply
		std::cout << "opcode " << (int)*(pkt_data + 21) << std::endl;
		if (*(pkt_data + 21) == 2)
		{
			// get mac and ip from packet data
			std::string mac = conv_mac_bytes_to_str((unsigned char *)(pkt_data + 22));
			std::string ip = conv_ip4_bytes_to_str((unsigned char *)(pkt_data + 28));

			mac_ip.insert(std::make_pair(mac, ip));

			std::cout << mac << " " << ip << std::endl;
		}
	}
}