S3P: Simple Serial Space Protocol
=================================

Welcome to S3P (Simple Serial Space Protocol), a lightweight serial
communication protocol designed for telemetry and telecommand (TM/TC) in
space hardware.

S3P provides a simple, reliable, and efficient solution for wired
communication in spacecraft, CubeSats, and other space applications.

Developed by Paradigma Technologies, S3P is open-source and available
under the MIT License to support the space community, including
researchers, hobbyists, and commercial developers.


Table of Contents
-----------------

- Overview (#overview)
- Key Features (#key-features)
- Use Cases (#use-cases)
- Getting Started (#getting-started)
- Installation (#installation)
- Usage (#usage)
- S3P shell (#s3psh)
- Contributing (#contributing)
- License (#license)
- Contact (#contact)


Overview
--------

S3P is a lightweight serial protocol optimized for space hardware,
offering a balance of simplicity and reliability for TM/TC applications.

S3P is tailored for the unique constraints of space environments, such
as small footprint, packet integrity, and compatibility with
microcontrollers and FPGAs.


Key Features
------------

- Lightweight: Minimal resource usage for constrained space systems.
- Packet Integrity: Robust error-checking for reliable data transfer in
    harsh environments.
- Serial Standards: Supports RS422 and RS485 in both full-duplex and
    half-duplex configurations.
- Flexible: Compatible with low memory MCUs and FPGAs.
- Open-Source: Freely available under the MIT License for community use
    and adaptation.


Use Cases
---------

- Telemetry data transmission in CubeSats and small satellites.
- Telecommand for onboard subsystems in spacecraft.
- Interfacing sensors and actuators in space hardware via RS422 or RS485.
- Prototyping and testing TM/TC systems in academic or commercial projects.


Getting Started
---------------

To use S3P, you’ll need:
- A microcontroller or FPGA supporting serial communication (e.g., Arduino, STM32, or Xilinx).
- Hardware supporting RS422 or RS485 interfaces (full-duplex or half-duplex).
- Basic knowledge of serial protocols and embedded systems.
- The S3P reference implementation (available in this repository)
    (or you can implement your own using the specification in doc/)


Prerequisites
-------------

- Compiler or IDE (e.g., GCC, PlatformIO) for C-based implementations.
- RS422 or RS485 transceivers (e.g., MAX485, SN65HVD series) for physical layer communication.
- Add any specific hardware or software requirements for S3P.


Installation
------------

Clone the repository:

- git clone https://github.com/Paradigma-Technologies/S3P.git
- Include the S3P library or code in your project.
- Configure your hardware for RS422 or RS485 communication (see Usage
    (#usage) for details).


Usage
-----

- Prepare an outgoing request packet:

        packet_t pkt_out;
        s3p_init_pkt_out(&pkt_out, <OUR_ID>, <NODE_ID>, <SEQ_NUM>);

- Populate packet Data (payload) section as per specification (e.g. for
    the Exec Command request)

        int data_len = 0;
        uint32_t arg = 0;
        // Cmd id
        pkt_out.data[data_len++] = cmd_id;
        // Arg
        pkt_out.data[data_len++] = (uint8_t)(arg >> 24);
        pkt_out.data[data_len++] = (uint8_t)(arg >> 16);
        pkt_out.data[data_len++] = (uint8_t)(arg >> 8);
        pkt_out.data[data_len++] = (uint8_t)arg;
        // Header
        pkt_out.data_len = data_len;
        pkt_out.type = PT_EXEC_CMD;

- Create serial frame (NOTE: ser_buf will be used for outgoing request
    and incoming response frame):

        ser_buf[S3P_MAX_RX_TX_SIZE];
        int size = s3p_make_frame(ser_buf, &pkt_out);
        // Check size
        if (!size)
            return;

- Write to serial device (serial port initialization not shown here):

        ser_write(&ser, ser_buf, size);

- Wait for response and optionally check for sequence correctness :

        // Note: wait_response will save incoming data into ser_buf and set
        // a pointer to the received Data (payload) in pkt_in
        packet_t pkt_in;
        if (!wait_response(&pkt_in))
            return;
        if (!check_seq(&pkt_in))
            return;

- Check response code and other data if available

        code = pkt_in.data[0];
        if (code != S3P_ERR_NONE) {
            printf("Read error: %s (%u)\n", s3p_err_str(code), code);
            return;
        }

For a complete example and implementation, see the s3psh/ folder


S3Psh
-----

A S3P shell is provided as a tool for connecting and managing remote
nodes via S3P procol and can be fund in the s3psh/ folder.

This is also usefull as a reference implementation of the client-side of
the protocol, and is always up-to-date to the specification in doc/

### Building

- Optional prerequisite: install your distribuition libreadline
    development package (if not availble, comment the corresponding line
    in Makefile and comment `#define USE_READLINE` in s3psh.c
- `cd s3psh`
- `make`
- `./s3psh -i <REMOTE_ID> -m <LOCAL_ID> /dev/ttyUSB0`


Contributing
------------

We welcome contributions from the space community! To contribute:

- Fork this repository.
- Create a feature branch (git checkout -b feature/your-feature).
- Commit your changes (git commit -m "Add your feature").
- Push to the branch (git push origin feature/your-feature).
- Open a Pull Request.

Please read CONTRIBUTING.md for guidelines on code style, testing and
submitting changes.


License
-------

S3P is licensed under the MIT License (LICENSE). You are free to use,
modify, and distribute this software, provided you include the license
and copyright notice.
Copyright © 2025 Paradigma Technologies Telekomunikacijske Tehnologije
d.o.o. (paradigma-tech.com) 

### References/Other libraries

- For COBS, Craig McQueen library has been used, see COBS_LICENSE.txt
  and [COBS-C](https://github.com/cmcqueen/cobs-c)


Contact
-------

For questions, feedback, or collaboration opportunities, reach out to us:

- Website: paradigma-tech.com
- Email: info@paradigma-tech.com
- We’re excited to see S3P empower the space community!



