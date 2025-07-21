S3P: Simple Space Serial Protocol
=================================


Introduction
------------

S3P is a serial communication protocol that can be used in buses with
with multiple (>2) participants.

S3P networks ara always composed of a single manager and one or more
controlled nodes.

Nodes (and manager) can have any address in the range 1-254
(0 and 255 are reserved).

All the transactions with S3P are always composed by a request (from the
manager) and a response (frome the node).

A response is always expected if the request type is supported, and if
not received within a certain timeout, a timeot error has occoured.

If the request type is not supported, no reponse from node will be sent.

Arbitration is not neede because only the manager can initiate a
communication with a node, and only a contacted node can reply to the
master within a certain timeout.

The physical layer is implemented according to EIA-422/EIA-485.


Contents
--------

The following documentation is divided in two main sections:

1. Network (Frame) and Transport (Packet) layers specification

2. ParadigmaTech custom Payload (command, responses, errors, etc)
   specification



1. Network & Transport
======================


Frame Specification
-------------------

Communication in S3p is based on frames (usually transmitted over a
serial line)

A frame consists of a packet followed by an end-of-frame delimiter.

```
.--------------.
|     Frame    |
|--------+-----|
| Packet | EOF |
'--------'-----'
```

S3P uses single byte with value `0x00` as EOF marker, and becaouse of
that, packet data must be escaped (byte stuffed) such that the EOF
marker does not occur in packet data.

[COBS Consistent Overhead Byte Stuffing](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
was choosen as byte stuffing algorithm due to its low overhead, low
memory footprint and simple and easy available implementations in
different programming languages.

Frame size has been set to 1024 bytes, which in our test has been proven
to be a good compromise between memory consumption, latency and
performances.

With a COBS encoded frame size of 1024 bytes, the maximum overhead due
to encoding can be calculated as:

    1024 / 254 + 1 + EOF = 6

This results in a maximum unencoded packet(payload) size of 1018 bytes.



Packet Specification
--------------------

```
.------.------.----------.------.--------.--------.-------.
|   1  |   1  |     1    |   1  |    2   | <=1010 |   2   |
.------+------+----------+------+--------+--------+-------.
| Src  | Dst  | RES/Seq  | Type | Lenght |  Data  | CRC16 |
'------'------'----------'------'--------'--------'-------'
```

- **Src**: source node Id for the packet (usually manager Id) (1-254)
- **Dst**: destionation node Id for the packet (node) (1-254)
- **Type**: packet type, which also determines the format of Data part
- **Lenght**: lenght of the Data part of the packet (i.e. header and
    CRC16 are not included)
- **CRC16**: CRC16 for the full packet, header included (i.e. from Src
    to Data end). Polynomial is of type CCITT_1D0F
- **RES/Seq**: the MSB (bits 7:4) are reserved and should always set to 0.
    bits 3:0 are used to as an increment counter (0 to 15) for a
    sequence number and used only by the manager, i.e. is the manager
    that changes (increment) the sequence number in the request to the
    Dst node. The node simply copies back the sequence number into the
    reply and sends it back to the manager. The manager could then check
    that the received packet has the correct sequnce number (i.e. no
    missing or duplicated packets). Therfore, the use of the sequence
    field is optional and at discretion of the manager.

        .----------.----------.
        | BIT 7:4  | BIT 3:0  |
        |----------+----------|
        | RESERVED | Sequence |
        '----------'----------'



2. ParadigaTech Payload Specification
=====================================

This section outlines the specifications for the 'Data' part (also known
as Payload) of a packet.


Commands/Responses
------------------

In the following documentation, the packet Type field is speficied
between square brackets for every request and response.


### Error Codes

Those are all the possible response error codes:

- S3P_ERR_NONE       =   0    no error
- S3P_ERR_VMEM_XLATE = 100    VMEM translation error (address OOB, size too big, etc)
- S3P_ERR_NO_REG     = 101    register does not exist
- S3P_ERR_NO_LOCK    = 102    error locking the registers table
- S3P_ERR_TYPE       = 103    type mismatch
- S3P_ERR_SIZE       = 104    packet size is different than expected
- S3P_ERR_NO_WRITE   = 105    request wrong data size (too short or too long)
- S3P_ERR_NO_VMEM    = 106    requested a VMEM mapping idx not present



### Execute Command
#### [0x10] Request

- Exec specific custom/debug commands with a single argument

```
.---------.----------.
|    1    |     4    |
|---------+----------|
| Command | Command  |
| Id      | Argument |
'---------'----------'
```

- **Command Id**: Id of the command to be executed
- **Command Argument**: single command argument. Format is command
    specific (int8/uint8, int16/uint16, int32/uint32, float)
- **Commands Table**:
    - CT_PING  =  0x10    ping node, arg not used (set to 0)
    - CT_REBOT =  0x11    reboot node, arg not used (set to 0)


#### [0x11] Response

```
.-------------.
|     1       |
|-------------|
| Execution   |
| Result Code |
'-------------'
```

- **Execution Result Code**: result code of command request
- **Result Codes Table**:
    - 0     = No error
    - Other = Implementation specific



### Read Register

- This request allow to read one or more registers from the slave
- Every register has a fixed size of 4 bytes
- Possible register types are: int8/uint8, int16/uint16, int32/uint32, float
- Types with size less of 4 bytes are right aligned


#### [0x12] Request

```
.----------.-------------.
|     2    |     2       |
|----------+-------------|
| First    | (N)umber of |
| Register | Registers   |
| Id       | to Read     |
'----------'-------------'
```

- **First Register Id**: Id of the first register to read
- **Number of Registers to Read**: total number of consecutive registers
    to read


#### [0x13] Response

```
.-----------.--------------------------------------------.
|     1     |                   N * 7                    |
|-----------+----------.-------.----------|----|----|----|
| Read      |     2    |   1   |    4     |    |    |    |
| Result    |----------+-------+----------|    |    |    |
| Code      | Register | Value | Register |    |    |    |
|           | Id       | Type  | Value    |    |    |    |
'-----------'----------'-------'----------'----'----'----'
```

- **Read Result Code**: result code of read request, see Error Codes
- **Register Id**: id of following register
- **Value Type**: Type of following value. One of:
    - EMPTY =  0    invalid value
    - U8    =  1    uint8
    - I8    =  2    int8
    - X8    =  3    U8 but showed as hex value (bitmasks)
    - U16   =  4    uint16
    - I16   =  5    int16
    - X16   =  6    U16 but showed as hex value (bitmasks)
    - U32   =  7    uint32
    - I32   =  8    int32
    - X32   =  9    U32 but showed as hex value (bitmasks)
    - FLT   = 10    float
    - STR   = 11    string (see Read String Register below)
- **Register Value**: register valure, right aligned (i.e. MSB first)



### Write Register

- This request allow to write one register to the slave
- Register has a fixed size of 4 bytes
- Possible register types are: int8/uint8, int16/uint16, int32/uint32, float
- Types with size less of 4 bytes are right aligned


#### [0x14] Request

```
.----------.-------.----------.
|     2    |   1   |    4     |
|----------+-------+----------|
| Register | Value | Register |
| Id       | Type  | Value    |
'----------'-------'----------'
```

- **Register Id**: Id of the register to write
- **Value Type**: see Read Register
- **Register Value**: register valure, right aligned (i.e. MSB first)


#### [0x15] Response

```
.-------------.
|     1       |
|-------------|
| Write       |
| Result Code |
'-------------'
```

- **Write Result Code**: result code of write request, see Error Codes



### Read Virtual Memory

- Virtual Memory (VMEM) is a 4GB addressable space that could be
    remapped to different phisical memories
- VMEM can be accessed by any address into the choosen mapping range
- Different address ranges access different memories
- VMEM mappings depend on the node configuration and can be retreived
    and listed using Get Virtual Memory Mapping commands


#### [0x16] Request

```
.---------.-------.
|     4   |   2   |
|---------+-------|
| VMEM    | Read  |
| Address | Size  |
'---------'-------'
```

- **VMEM Address**: address to read from in virtual memory
- **Read Size**: requested read size. Response could be smaller. Maximum
    reply size is limited to 1004 bytes
  If data greater than this must be read, the caller must call this
  function multiple times incrementing Address and setting Read Size to
  MIN(1004, remaining_bytes)


#### [0x17] Response

```
.-------------.---------------.
|     1       |     <= 1004   |
|-------------|---------------|
| Read        |     Buffer    |
| Result Code |               |
'-------------'---------------'
```

- **Read Result Code**: result code of read request, see Error Codes
- **Buffer**: 0 to 1004 bytes of data (0 if Read Result Code is !=0)

Note: Buffer section size corresponds to Packet Lenght - 1 (Result Code
size)



### Write Virtual Memory

- See previous section for Virtual Memory description


#### [0x18] Request

```
.---------.---------------.
|     4   |     <= 1004   |
|---------|---------------|
| VMEM    |     Buffer    |
| Address |               |
'---------'---------------'
```

- **VMEM Address**: address to write to in virtual memory
- **Buffer**: 0 to 1004 bytes of data (0 if Read Result Code is !=0)

Note: Buffer section size corresponds to Packet Lenght - 4 (Address size)
  If data greater than this must be written, the caller must call this
  function multiple times incrementing Address and poulating Buffer
  section with remaining data


#### [0x19] Response

```
.-------------.
|     1       |
|-------------|
| Write       |
| Result Code |
'-------------'
```

- **Write Result Code**: result code of read request, see Error Codes



### Read String Register

- This request allow to read string registers


#### [0x1A] Request

```
.----------.
|     2    |
|----------|
| Register |
| Id       |
'----------'
```

- **Register Id**: Id of the register to read


#### [0x1B] Response

```
.-------------.----------.-------.-------------------.
|     1       |     2    |   1   |      <= 1006      |
|-------------+----------|-------+-------------------|
| Read        | Register | Value | Null terminated   |
| Result Code | Id       | Type  | string            |
'-------------'----------'-------'-------------------'
```

- **Register Id**: Id of the register
- **Read Result Code**: result code of write request, see Error Codes
- **Null terminated string**: requested string



### Write String Register

- This request allow to write string registers


#### [0x1C] Request

```
.----------.-------------------.
|     2    |      <= 1009      |
|----------|-------------------|
| Register | Null terminated   |
| Id       | string            |
'----------'-------------------'
```

- **Register Id**: Id of the register to read
- **Null terminated string**: requested string


#### [0x1D] Response

```
.-------------.
|     1       |
|-------------|
| Write       |
| Result Code |
'-------------'
```

- **Write Result Code**: result code of write request, see Error Codes



### Get S3P Info

- This request allow to get general information about S3P version used
    on the node, number of registers, VMEM, etc


#### [0x30] Request

```
.----------.
|     1    |
|----------|
| Reserved |
| (set 0)  |
'----------'
```

- **Reserved**: reserved field, set to 0


#### [0x31] Response

```
.--------.----------.----------.----------.-----------.------------.
|    1   |     2    |     2    |     2    |     2     |     1      |
|--------+----------+----------+----------+-----------+------------|
| Info   | S3P      | Register | Register | Registers | VMEM       |
| Result | Protocol | Min      | Max      | Count     | Mappings   |
| Code   | Version  | Id       | Id       |           | Count      |
'--------'----------'----------'----------'-----------'------------'
```

- **Info Result Code**: result code of read request, see Error Codes
- **S3P Protocol Version**: version of the protocol supported by the
    node, as a two byte hex number in the form 0xMMmm (MM major version,
    mm minor version), for example 0x0100 for version 1.00
- **Register Min Id**: minimum accesible register id
- **Register Max Id**: minimum accesible register id
- **Registers count**: total count of available resgisters. NOTE: due to
    gaps in the register map, count is usually less than max-min
- **VMEM Mappings Count**: total count of VMEM mapping table rows.
    0 if VMEM is not supported



### Get Register Info

- This request allow to get information about a specific register


#### [0x32] Request

```
.----------.
|     2    |
|----------|
| Register |
| Id       |
'----------'
```

- **Register Id**: Id of the register to get info


#### [0x33] Response

```
.--------.----------.----------.-------.----------.----------.------------.
|    1   |     2    |     2    |   1   |    1     |     2    |  <= 32     |
|--------+----------+----------+-------+----------+----------+------------|
| Info   | Register | Next     | Value | Register | Register | Register   |
| Result | Id       | Register | Type  | Group    | Flags    | Name (null |
| Code   |          | Id       |       |          |          | term str)  |
'--------'----------'----------'-------'----------'----------'------------'
```

- **Info Result Code**: result code of read request, see Error Codes
- **Register Id**: Id of the register
- **Next Register Id**: Id of the next valid register (for skipping gaps),
  0 if no more registers are available
- **Value Type**: see read register response [0x13]
- **Register Group**: optional register group id (like system,
    telemetry, calibration, etc). Those values are mission specific and
    are available upon request to ParadigmaTech
- **Register Flags**: register flags
    - F_NONE    = 0x0000  no flags set
    - F_MUTABLE = 0x0001  mutable (R/W)
    - F_PERSIST = 0x0002  persistent (saved across reboots)
- **Register name**: null terminated string with register name



### Get Virtual Memory Mapping

- This request allow to get information about the virtual memory space
  mapping


#### [0x34] Request

```
.----------.
|     2    |
|----------|
| Mapping  |
| Index    |
'----------'
```

- **Mappin Index**: index of the remote mappings table to retrive,
  starting from 0 to VMEM Map Table Rows Count (see Get Table Info)


#### [0x35] Response

```
.--------.------.-------.--------.----------.----------.--------.--------.------------.
|    1   |   1  |    1  |    1   |    4     |     4    |    1   |    1   |  <= 32     |
|--------+------+-------+--------+----------+----------+--------+--------+------------|
| Info   | Row  | Next  | Memory | Virtual  | Region   | Flags  | Mirror | Region     |
| Result | Idx  | Row   | Type   | Region   | Size     |        | Memory | Name (null |
| Code   |      | Idx   |        | Address  |          |        | Type   | term str)  |
'--------'------'-------'--------'----------'----------'--------'--------'------------'
```

- **Info Result Code**: result code of read request, see Error Codes
- **Row Idx**: Idx of the received row
- **Next Row Idx**: Idx of the next valid row, 0 for end of table
- **Memory Type**: memory type id. Those values are mission specific and
    are available upon request to ParadigmaTech
- **Virtual Region Address**: address to use to access the region
- **Region Size**: size of the mapped region
- **Region Flags**: flags/properties of the region
    - VF_NONE   = 0x00  no flags set
    - VF_READ   = 0x01  readable region
    - VF_WRITE  = 0x02  writable region
    - VF_MIRROR = 0x04  region is mirrored on another address and/or memory
- **Mirror Memory Type**: type or memory used for mirrorion (if applicable)
- **Region name**: null terminated string with register name



Changelog
=========

v1.0
----

- Initial release

