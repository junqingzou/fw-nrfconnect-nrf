.. _eureka_lwm2m_proxy	:

nRF9160: Serial LTE Modem
#########################

The Eureka LwM2M Proxy (ELP) demonstrates how to expose LwM2M on nRF91 DK to
external MCU, over a simple UART-based protocol. The specification of this
project is found under "doc" folder.

Overview
********

The Eureka LwM2M Proxy exposes below services on nRF91 side.

.. list-table::
   :align: center

   * - Modem control
     - Supported, optional
     - Service provided by LTE_LC driver
   * - LwM2M service
     - Supported, optional
     - Service provided by Zephyr LwM2M Lib

Requirements
************

* The following development board:

    * nRF9160 DK board (PCA10090)

* To test with nRF52 clients

    * nRF52840 DK board (PCA10056)
    * nRF52832 DK board (PCA10040)

The pin inter-connection is shown as in below table

.. list-table::
   :align: center

   * - nRF52 DK
     - nRF91 DK
   * - UART TX P0.6
     - UART RX P0.11
   * - UART RX P0.8
     - UART TX P0.10
   * - UART CTS P0.7
     - UART RTS P0.12
   * - UART RTS P0.5
     - UART RTS P0.13
   * - GPIO OUT P0.27
     - GPIO IN P0.31

* UART instance in use
   * nRF52840 nRF52832 (UART0)
   * nRF9160 (UART2)

* UART configuration
   * Hardware flow control:	enabled
   * Baud-rate: 115200
   * Parity bit: no
   * Operation mode: IRQ

Please NOTE the GPIO output level on nRF91 side should be 3V.

Building and Running
********************

There are configuration files for various setups in the
samples/nrf9160/serial_lte_modem	directory:

- :file:`prj.conf`
  This is the standard default config.
- :file:`child_secure_partition_manager.conf`
  This is the project-specific SPM config.

The easiest way to setup this sample application is to build and run it
on the nRF9160-DK board using the default configuration :file:`prj.conf`.

NOTE when wakeup is enabled, nRF91 start up in Sleep and external MCU
needs to wake it up by GPIO.

This project works with nRF52 client. Sample nRF52 client projects:
https://github.com/NordicPlayground/nrf5-serial-lte-modem-clients

If CONFIG_ELP_TEST is set, limited test is possible without a nRF52 client.

Security Support
****************

Currently the MQTT and LwM2M service have no security support yet.

Dependencies
************

This application uses the following |NCS| libraries and drivers:

    * ``nrf/drivers/lte_link_control``
    * ``nrf/drivers/nrf9160_gps``
    * ``nrf/lib/bsd_lib``
    * ``zephyr/subsys/net/lib/mqtt``
    * ``zephyr/subsys/net/lib/lwm2m``
    * ``zephyr/subsys/net/lib/sntp``

In addition, it uses the Secure Partition Manager sample:

* :ref:`secure_partition_manager`

References
**********

* MQTT:
http://www.steves-internet-guide.com/mqtt-publish-subscribe/

* LwM2M:
https://www.omaspecworks.org/what-is-oma-specworks/iot/lightweight-m2m-lwm2m/

* Leshan Demo Server
https://github.com/eclipse/leshan/blob/master/README.md
