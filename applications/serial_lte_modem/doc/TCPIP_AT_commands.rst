.. _SLM_AT_TCP_UDP:

TCP and UDP AT commands
***********************

.. contents::
   :local:
   :depth: 2

The following commands list contains TCP and UDP related AT commands.

For more information on the networking services, visit the `BSD Networking Services Spec Reference`_.

TCP server #XTCPSVR
===================

The ``#XTCPSVR`` command allows you to start and stop the TCP server.

Set command
-----------

The set command allows you to start and stop the TCP server.

Syntax
~~~~~~

::

   #XTCPSVR=<op>[<port>[,<sec_tag>]]


* The ``<op>`` parameter can accept one of the following values:

  * ``0`` - Stop the server
  * ``1`` - Start the server for IP protocol family version 4
  * ``2`` - Start the server with data mode support for IP protocol family version 4
  * ``3`` - Start the server for IP protocol family version 6
  * ``4`` - Start the server with data mode support for IP protocol family version 6

* The ``<port>`` parameter is an unsigned 16-bit integer (0 - 65535).
  It represents the TCP service port.
  It is mandatory to set it when starting the server.
* The ``<sec_tag>`` parameter is an integer.
  It indicates to the modem the credential of the security tag used for establishing a secure connection.

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPSVR: <handle>,"started"

The ``<handle>`` value is an integer.
When positive, it indicates that it opened successfully.
When negative, it indicates that it failed to open.

Unsolicited notification
~~~~~~~~~~~~~~~~~~~~~~~~

::

   #XTCPSVR: <error>,"stopped"

* The ``<error>`` value is a negative integer.
  It represents the error value according to the standard POSIX *errorno*.

::

   #XTCPDATA: <size>

* The ``<size>`` value is the length of RX data received by the SLM waiting to be fetched by the MCU.

Examples
~~~~~~~~

::

   at#xtcpsvr=1,3442,600
   #XTCPSVR: 2,"started"
   OK
   #XTCPSVR: "5.123.123.99","connected"
   Hello, TCP#1!
   #XTCPDATA: 13
   Hello, TCP#2!
   #XTCPDATA: 13

Read command
------------

The read command allows you to check the TCP server settings.

Syntax
~~~~~~

::

   #XTCPSVR?

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPSVR: <listen_socket_handle>,<income_socket_handle>,<family>,<data_mode>

* The ``<xxx_socket_handle>`` value is an integer.
  When positive, it indicates that it opened successfully.
  When negative, it indicates that it failed to open or that there is no incoming connection.

* The ``<family>`` value is an integer.
 
  *``1`` - IP protocol family version 4
  *``2`` - IP protocol family version 6

* The ``<data_mode>`` value can assume one of the following values:

  * ``0`` - Disabled
  * ``1`` - Enabled

Examples
~~~~~~~~

::

   at#xtcpsvr?
   #XTCPSVR: 1,2,1,0
   OK

Test command
------------

The test command tests the existence of the command and provides information about the type of its subparameters.

Syntax
~~~~~~

::

   #XTCPSVR=?

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPSVR: (list of op value),<port>,<sec_tag>

Examples
~~~~~~~~

::

   AT#XTCPSVR=?
   #XTCPSVR: (0,1,2,3,4),<port>,<sec_tag>
   OK

TCP/TLS client #XTCPCLI
=======================

The ``#XTCPCLI`` command allows you to create a TCP/TLS client and to connect to a server.

Set command
-----------

The set command allows you to create a TCP/TLS client and to connect to a server.

Syntax
~~~~~~

::

   #XTCPCLI=<op>[,<url>,<port>[,[sec_tag]]

* The ``<op>`` parameter can accept one of the following values:

  * ``0`` - Disconnect
  * ``1`` - Connect to the server for IP protocol family version 4
  * ``2`` - Connect to the server with data mode support for IP protocol family version 4
  * ``3`` - Connect to the server for IP protocol family version 6
  * ``4`` - Connect to the server with data mode support for IP protocol family version 6

* The ``<url>`` parameter is a string.
  It indicates the hostname or the IP address to connect to.
  Its maximum size is 128 bytes.
  When the parameter is an IP address, it supports IPv4 only, not IPv6.
* The ``<port>`` parameter is an unsigned 16-bit integer (0 - 65535).
  It represents the TCP/TLS service port.
  It is mandatory for starting the server.
* The ``<sec_tag>`` parameter is an integer.
  It indicates to the modem the credential of the security tag used for establishing a secure connection.

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPCLI: <handle>, "connected"

Unsolicited notification
~~~~~~~~~~~~~~~~~~~~~~~~

::

   #XTCPCLI: <error>, "disconnected"

* The ``<error>`` value is a negative integer.
  It represents the error value according to the standard POSIX *errorno*.

When TLS/DTLS is expected, the credentials should be stored on the modem side by ``AT%XCMNG`` or by the Nordic nRF Connect/LTE Link Monitor tool.
The modem needs to be in the offline state.

::

   #XTCPDATA: <size>

* The ``<size>`` value is the length of RX data received by the SLM waiting to be fetched by the MCU.

Examples
~~~~~~~~

::

   AT#XTCPCLI=1,"remote.ip",1234
   #XTCPCLI: 2,"connected"
   OK
   PONG: b'Test TCP by IP address'
   #XTCPDATA: 31

   at#xtcpcli=0
   OK

Read command
------------

The read command allows you to verify the status of the connection.

Syntax
~~~~~~

::

   #XTCPCLI?

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPCLI: <handle>,<family>,<data_mode>

* The ``<handle>`` value is an integer.
  When positive, it indicates that it opened successfully.
  When negative, it indicates that it failed to open.

* The ``<family>`` value is an integer.
 
  *``1`` - IP protocol family version 4
  *``2`` - IP protocol family version 6

* The ``<data_mode>`` value can assume one of the following values:

  * ``0`` - Disabled
  * ``1`` - Enabled

Test command
------------

The test command tests the existence of the command and provides information about the type of its subparameters.

Syntax
~~~~~~

::

   #XTCPCLI: (op list),<url>,<port>,<sec_tag>

Examples
~~~~~~~~

::

   at#xtcpcli=?
   #XTCPCLI: (0,1,2,3,4),<url>,<port>,<sec_tag>
   OK

TCP send data #XTCPSEND
=======================

The ``#XTCPSEND`` command allows you to send the data over the connection.

Set command
-----------

The set command allows you to send the data over the connection.
When used from a TCP/TLS client, it sends the data to the remote TCP server
When used from a TCP server, it sends data to the remote TCP client

Syntax
~~~~~~

::

   #XTCPSEND=<data>

* The ``<data>`` parameter is a string.
  It contains the data being sent.
  The maximum size for ``NET_IPV4_MTU`` is 576 bytes.
  It should have no ``NULL`` character in the middle.

Response syntax
~~~~~~~~~~~~~~~

::

   #XTCPSEND: <size>

* The ``<size>`` value is an integer.
  It represents the actual number of the bytes sent.

Examples
~~~~~~~~

::

   at#xtcpsend=1,"Test TLS client"
   #XTCPSEND: 15
   OK

Read command
------------

The read command is not supported.

Test command
------------

The test command is not supported.


UDP server #XUDPSVR
===================

The ``#XUDPSVR`` command allows you to start and stop the UDP server.

Set command
-----------

The set command allows you to start and stop the UDP server.

Syntax
~~~~~~

::

   #XUDPSVR=<op>[,<port>]

* The ``<op>`` parameter can accept one of the following values:

  * ``0`` - Stop the server
  * ``1`` - Start the server
  * ``2`` - Start the server with data mode support

* The ``<port>`` parameter is an unsigned 16-bit integer (0 - 65535).
  It represents the UDP service port.
  It is mandatory for starting the server.
  The data mode is enabled when the TCP/TLS server is started.

Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPSVR: <handle>,"started"

The ``<handle>`` value is an integer.
When positive, it indicates that it opened successfully.
When negative, it indicates that it failed to open.

Unsolicited notification
~~~~~~~~~~~~~~~~~~~~~~~~

::

   #XUDPSVR: <error>,"stopped"

The ``<error>`` value is a negative integer.
It represents the error value according to the standard POSIX *errorno*.

The reception of data is automatic.
It is reported to the client as follows:

::

   #XUDPDATA: <size>
   <data>

Examples
~~~~~~~~

::

   at#xudpsvr=1,3442
   #XUDPSVR: 2,"started"
   OK
   #XUDPDATA: 13
   Hello, UDP#1!
   #XUDPDATA: 13
   Hello, UDP#2!

Read command
------------

The read command allows you to check the current value of the subparameters.

Syntax
~~~~~~

::

   #XUDPSVR?

Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPSVR: <handle>,<data_mode>

The ``<handle>`` value is an integer.
When positive, it indicates that it opened successfully.
When negative, it indicates that it failed to open.

* The ``<data_mode>`` value can assume one of the following values:

  * ``0`` - Disabled
  * ``1`` - Enabled

Test command
------------

The test command tests the existence of the command and provides information about the type of its subparameters.

Syntax
~~~~~~

::

   #XUDPSVR=?

Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPSVR: (list of op value),<port>,<sec_tag>

Examples
~~~~~~~~

::

   at#xudpsvr=?
   #XUDPSVR: (0,1,2),<port>,<sec_tag>
   OK

UDP/DTLS client #XUDPCLI
========================

The ``#XUDPCLI`` command allows you to create a UDP/DTLS client and to connect to a server.

Set command
-----------

The set command allows you to create a UDP/DTLS client and connect to a server.

Syntax
~~~~~~

::

   #XUDPCLI=<op>[,<url>,<port>[,<sec_tag>]

* The ``<op>`` parameter can accept one of the following values:

  * ``0`` - Disconnect
  * ``1`` - Connect to the server
  * ``2`` - Connect to the server with data mode support

* The ``<url>`` parameter is a string.
  It indicates the hostname or the IP address to connect to.
  Its maximum size can be 128 bytes.
  When the parameter is an IP address, it supports IPv4 only, not IPv6.
* The ``<port>`` parameter is an unsigned 16-bit integer (0 - 65535).
  It represents the UDP/DTLS service port.
* The ``<sec_tag>`` parameter is an integer.
  It indicates to the modem the credential of the security tag used for establishing a secure connection.

Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPCLI: <handle>,"connected"

Unsolicited notification
~~~~~~~~~~~~~~~~~~~~~~~~

::

   #XUDPCLI: <error>,"disconnected"

The ``<error>`` value is a negative integer.
It represents the error value according to the standard POSIX *errorno*.

The reception of data is automatic.
It is reported to the client as follows:

::

   #XUDPDATA: <size>
   <data>

Examples
~~~~~~~~

::

   at#xudpcli=1,"remote.host",2442
   #XUDPCLI: 2,"connected"
   OK
   at#xudpsend=1,"Test UDP by hostname"
   #XUDPSEND: 20
   OK
   #XUDPDATA: 26
   PONG: Test UDP by hostname
   at#xudpcli=0
   OK

Read command
------------

The read command allows you to check the current value of the subparameters.

Syntax
~~~~~~

::

   #XUDPCLI?

Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPCLI: <handle>,<data_mode>

The ``<handle>`` value is an integer.
When positive, it indicates that it opened successfully.
When negative, it indicates that it failed to open.

* The ``<data_mode>`` value can assume one of the following values:

  * ``0`` - Disabled
  * ``1`` - Enabled

Test command
------------

The test command tests the existence of the command and provides information about the type of its subparameters.

Syntax
~~~~~~

::

   #XUDPCLI: (op list),<url>,<port>,<sec_tag>

Examples
~~~~~~~~

::

   at#xudpcli=?
   #XUDPCLI: (0,1,2),<url>,<port>,<sec_tag>
   OK

UDP send data #XUDPSEND
=======================

The ``#XUDPSEND`` command allows you to send data over the connection.

Set command
-----------

The set command allows you to send data over the connection.

Syntax
~~~~~~

::

   #XUDPSEND=<data>

* The ``<data>`` parameter is a string type.
  It contains arbitrary data.


Response syntax
~~~~~~~~~~~~~~~

::

   #XUDPSEND: <size>

* The ``<size>`` value is an integer.
  It indicates the actual number of bytes sent.

Examples
~~~~~~~~

::

   at#xudpsend=1,"Test UDP by hostname"
   #XUDPSEND: 20
   OK

Read command
------------

The read command is not supported.

Test command
------------

The test command is not supported.
