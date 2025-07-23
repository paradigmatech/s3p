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

- [Overview](#overview)
- [Key Features](#key-features)
- [Use Cases](#use-cases)
- [Getting Started](#getting-started)
- [Installation](#installation)
- [Usage](#usage)
- [S3P shell](#s3psh)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

<a name="overview"></a>
Overview
--------

S3P is a lightweight serial protocol optimized for space hardware,
offering a balance of simplicity and reliability for TM/TC applications.

S3P is tailored for the unique constraints of space environments, such
as small footprint, packet integrity, and compatibility with
microcontrollers and FPGAs.


<a name="key-features"></a>
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


<a name="use-cases"></a>
Use Cases
---------

- Telemetry data transmission in CubeSats and small satellites.
- Telecommand for onboard subsystems in spacecraft.
- Interfacing sensors and actuators in space hardware via RS422 or RS485.
- Prototyping and testing TM/TC systems in academic or commercial projects.


<a name="getting-started"></a>
Getting Started
---------------

To use S3P, you’ll need:
- A microcontroller or FPGA supporting serial communication (e.g., Arduino, STM32, or Xilinx).
- Hardware supporting RS422 or RS485 interfaces (full-duplex or half-duplex).
- Basic knowledge of serial protocols and embedded systems.
- The S3P reference implementation (available in this repository)
    (or you can implement your own using the specification in doc/)


<a name="prerequisites"></a>
Prerequisites
-------------

- Compiler or IDE (e.g., GCC, PlatformIO) for C-based implementations.
- RS422 or RS485 transceivers (e.g., MAX485, SN65HVD series) for physical layer communication.
- Add any specific hardware or software requirements for S3P.


<a name="installation"></a>
Installation
------------

Clone the repository:

- git clone https://github.com/Paradigma-Technologies/S3P.git
- Include the S3P library or code in your project.
- Configure your hardware for RS422 or RS485 communication (see Usage
    (#usage) for details).


<a name="usage"></a>
Usage
-----

- Prepare an outgoing request packet:

        static uint8_t pkt_out_buf[S3P_MAX_PKT_SIZE];
        packet_t pkt_out;
        s3p_init_pkt(&pkt_out, pkt_out_buf, <OUR_ID>, <NODE_ID>, <SEQ_NUM>);

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

- Create serial frame (NOTE: frame_buf will be used for outgoing request
    and incoming response frame):

        frame_buf[S3P_MAX_RX_TX_SIZE];
        int size = s3p_make_frame(frame_buf, &pkt_out);
        // Check size
        if (!size)
            return;

- Write to serial device (serial port initialization not shown here):

        ser_write(&ser, frame_buf, size);

- Wait for a full frame response and pares it into a packet:

        // Example of wait_response() function
        static bool wait_response(s3p_packet_t *pkt_in)
        {
            static uint8_t pkt_in_buf[S3P_MAX_PKT_SIZE];
            int nbytes;
            uint8_t byt;
            uint16_t rx_len = 0;
            timeout_start()
            while (!timeout_elapsed())) ) {
                nbytes = ser_read(&ser, &byt, 1);
                if (!nbytes) {
                    usleep(BYTE_DELAY);
                    continue;
                }
                if (rx_len < S3P_MAX_FRAME_SIZE)
                    frame_buf[rx_len++] = byt;

                if (byt == S3P_COBS_DELIM) {
                    // Init packet with packet buffer ptr and dummy values
                    s3p_init_pkt(pkt_in, pkt_in_buf, S3P_ID_NONE, S3P_ID_NONE,
                            S3P_SEQ_NONE);
                    // Parse received frame into packet
                    bool res = s3p_parse_frame(pkt_in, manager_id, frame_buf,
                            rx_len-1);
                    return res;
                }
            }
        }

        // Note: wait_response will save incoming data into frame_buf
        // and set a pointer to the received Data (payload) in pkt_in
        packet_t pkt_in;
        if (!wait_response(&pkt_in))
            return;

        // Optionally check for correct sequence number, i.e. that the
        // received sequence is the same as the sent sequence
        if (S3P_SEQ_MASKED(pkt_in.flags_seq) != <SEQ_NUM>) {
            // Sequence error
            return;
        }

- Check response code and deserialize other data/payload if available:

        code = pkt_in.data[0];
        if (code != S3P_ERR_NONE) {
            return;
        }

For a complete example and implementation, see the s3psh/ folder.

There is also a Doxygen project file under doc/ to generate basic
library documentation


<a name="s3psh"></a>
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


<a name="contributing"></a>
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


<a name="license"></a>
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


<a name="contact"></a>
Contact
-------

For questions, feedback, or collaboration opportunities, reach out to us:

- Website: paradigma-tech.com
- Email: info@paradigma-tech.com
- We’re excited to see S3P empower the space community!



