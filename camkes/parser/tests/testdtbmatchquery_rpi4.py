#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
# Copyright 2021, Technology Innovation Institute
#
# SPDX-License-Identifier: BSD-2-Clause
#
#

from __future__ import absolute_import, division, print_function, unicode_literals

from camkes.parser import ParseError, DtbMatchQuery
from camkes.internal.tests.utils import CAmkESTest
import os
import sys
import unittest

ME = os.path.abspath(__file__)

# Make CAmkES importable
sys.path.append(os.path.join(os.path.dirname(ME), '../../..'))


class TestDTBMatchQuery(CAmkESTest):
    '''
    As part of the changes brought by the new seL4DTBHardware connector,
    the DTB dictionary now includes keys 'this-address-cells' and 'this-size-cells'.
    The keys describe the number of cells required to express those particular fields in
    the 'reg' property of the DTB.

    As part of the changes brought by exposing the DTB to CAmkES userland, the DTB dictionary
    now includes a 'dtb_size' key. This key-value pair describes the size of the DTB, in bytes.
    '''

    def setUp(self):
        super(TestDTBMatchQuery, self).setUp()
        self.dtbQuery = DtbMatchQuery()
        self.dtbQuery.parse_args(['--dtb', os.path.join(os.path.dirname(ME), "test_rpi4.dtb")])
        self.dtbQuery.check_options()
        self.dtbSize = os.path.getsize(os.path.join(os.path.dirname(ME), "test_rpi4.dtb"))

    def test_aliases(self):
        node = self.dtbQuery.resolve([{'aliases': 'ethernet0'}])
        self.assertIsInstance(node, dict)

        expected = {
            'compatible': ["brcm,bcm2711-genet-v5"],
            'reg': [0xfd580000, 0x10000],
            '#address-cells': [0x01],
			'#size-cells': [0x01],
            'interrupts': [0x00, 0x9d, 0x04, 0x00, 0x9e, 0x04],
            'status': "okay",
            'phy-handle': [0x26],
            'phy-mode': "rgmii-rxid",
            'this-address-cells': [0x2],
            'this-size-cells': [0x1],
            'this-node-path': "/scb/ethernet@7d580000",
        }
        self.assertIn('query', node)
        self.assertIn('dtb-size', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


        node = self.dtbQuery.resolve([{'aliases': 'serial1'}])
        expected = {
            'compatible': ["brcm,bcm2835-aux-uart"],
            'reg': [0xfe215040, 0x40],
            'interrupts': [0x00, 0x5d, 0x04],
            'clocks': [0x0c, 0x00],
            'status': "okay",
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/serial@7e215040",
        }

        self.assertIn('query', node)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['dtb-size'], [self.dtbSize])

    def test_paths(self):
        node = self.dtbQuery.resolve([{'path': "/soc/interrupt-controller.*"}])
        expected = {
            '#interrupt-cells': [0x1],
            'compatible': ["arm,gic-400"],
            'reg': [0xff841000, 0x1000, 0xff842000, 0x2000, 0xff846000, 0x2000],
            'interrupts': [0x01, 0x09, 0xf04],
            'phandle': [0x01],
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/interrupt-controller@40041000",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)


        node = self.dtbQuery.resolve([{'path': '/soc/usb.*'}])
        expected = {
            'compatible': ["brcm,bcm2835-usb"],
            'reg': [0xfe980000, 0x10000],
            'interrupts': [0x00, 0x49, 0x04],
            '#address-cells': [0x1],
            '#size-cells': [0x0],
            'clocks': [0x12],
            'clock-names': "otg",
            'phys': [0x13],
            'phys-names': "usb2-phy",
            'dr_mode': "peripheral",
            'g-rx-fifo-size': [0x100],
            'g-np-tx-fifo-size ': [0x20],
            'g-tx-fifo-size': [0x100, 0x100, 0x200, 0x200, 0x200, 0x300, 0x300],
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/usb@7e980000",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


        node = self.dtbQuery.resolve([{'path': '/soc/mailbox.*b880'}])
        expected = {
            'compatible': ["brcm,bcm2835-mbox"],
            'reg': [0xfe00b880, 0x40],
            'interrupts': [0x00, 0x21, 0x04],
            '#mbox-cells': [0x00],
            'phandle': [0x1c],
            'this-address-cells': [0x01],
            'this-size-cells': [0x00],
            'this-node-path': "/soc/mailbox@7e00b880",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


    def test_multiple_queries(self):
        node = self.dtbQuery.resolve([{'path': "power"}, {'aliases': 'pcie0'}])
        expected = [
            {
                'compatible': ["raspberrypi,bcm2835-power"],
                'firmware': [0x1d],
                '#power-domain-cells': [0x1],
                'phandle': [0x0b],
                'this-address-cells': [0x1],
                'this-size-cells': [0x1],
                'this-node-path': "/soc/power",
            },
            {
                'compatible': ["brcm,bcm2711-pcie"],
                'reg': [0xfd500000, 0x9310],
                'device-type': "pci",
                '#address-cells': [0x3],
                '#interrupt-cells': [0x1],
                '#size-cells': [0x2],
                'interrupts': [0x00, 0x94, 0x04, 0x00, 0x94, 0x04],
                'interrupt-names': ["pcie", "msi"],
                'interrupt-map-mask': [0x00, 0x00, 0x00, 0x07],
                'interrupt-map': [0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x8f, 0x04],
                'msi-controller': "",
                'msi-parent': [0x24],
                'ranges': [0x2000000, 0x00, 0xf8000000, 0x06, 0x00, 0x00, 0x4000000],
                'dma-ranges': [0x2000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0000000],
                'brcm,enable-ssc': "",
                'phandle': [0x24],
                'this-address-cells': [0x2],
                'this-size-cells': [0x1],
                'this-node-path': "/scb/pcie@7d500000",
            }
        ]
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 2)
        self.assertEquals(node['query'][0], expected[0])
        self.assertEquals(node['query'][1], expected[1])

    def test_blank(self):
        self.assertRaises(ParseError, self.dtbQuery.resolve, [])

    def test_blank_query(self):
        node = self.dtbQuery.resolve([{}])
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], {})


    def test_properties_lvalue_index(self):
        node = self.dtbQuery.resolve([{'properties': {'reg[0]': 0x7e215040}}])
        expected = {
            'compatible': ["brcm,bcm2835-aux-uart"],
            'reg': [0xfe215040, 0x40],
            'interrupts': [0x00, 0x5d, 0x04],
            'clocks': [0x0c, 0x00],
            'status': "okay",
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/serial@7e215040",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])

        node = self.dtbQuery.resolve([{'properties': {'compatible[0]': 'brcm,bcm2835-dsi1'}}])
        expected = {
            'compatible': ['brcm,bcm2835-dsi1'],
            'reg': [0xfe700000, 0x8c],
            'interrupts': [0x00, 0x6c, 0x04],
			'#address-cells': [0x01],
			'#size-cells': [0x00],
			'#clock-cells': [0x01],
            'clocks': [0x06, 0x23, 0x06, 0x30, 0x06, 0x32],
            'clock-names': ["phy", "escape", "pixel"],
            'clock-output-names': ["dsi1_byte", "dsi1_ddr2", "dsi1_ddr"],
            'status': "disabled",
            'power-domains':  [0x0b, 0x12],
            'phandle': [0x05],
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/dsi@7e700000",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])

        node = self.dtbQuery.resolve([{'properties': {'compatible[0]': 'brcm,bcm2836-l1-intc'}}])
        expected = {
            'compatible': ['brcm,bcm2836-l1-intc'],
            'reg': [0x40000000, 0x100],
            'this-address-cells': [0x01],
            'this-size-cells': [0x01],
            'this-node-path': "/soc/local_intc@40000000",
        }
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        self.assertEquals(node['query'][0], expected)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


    def test_properties_star_string(self):
        node = self.dtbQuery.resolve([{
            'properties':  {
                "clock-output-names[*]": ["dsi0_byte", "dsi0_ddr2", "dsi0_ddr"]
            }
        }])
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        query = node['query'][0]

        self.assertIn('compatible', query)
        self.assertEquals(query['compatible'], ["brcm,bcm2835-dsi0"])

        self.assertIn('reg', query)
        self.assertEquals(query['reg'], [0xfe209000, 0x78])

        self.assertIn('interrupts', query)
        self.assertEquals(query['interrupts'], [0x00, 0x64, 0x04])

        self.assertIn('#address-cells', query)
        self.assertEquals(query['#address-cells'], [0x1])

        self.assertIn('#size-cells', query)
        self.assertEquals(query['#size-cells'], [0x0])

        self.assertIn('#clock-cells', query)
        self.assertEquals(query['#clock-cells'], [0x1])

        self.assertIn('clocks', query)
        self.assertEquals(query['clocks'], [0x06, 0x20, 0x06, 0x2f, 0x06, 0x31])

        self.assertIn('clock-names', query)
        self.assertEquals(query['clock-names'], ["phy", "esacpe", "pixel"])

        self.assertIn('clock-output-names', query)
        self.assertEquals(query['clock-output-names'], ["dsi0_byte", "dsi0_ddr2", "dsi0_ddr"])

        self.assertIn('status', query)
        self.assertEquals(query['status'], ["disabled"])

        self.assertIn('power-domains', query)
        self.assertEquals(query['power-domains'], [0x0b, 0x11])

        self.assertIn('phandle', query)
        self.assertEquals(query['phandle'], [0x4])

        self.assertIn('this-address-cells', query)
        self.assertEquals(query['this-address-cells'], [0x1])

        self.assertIn('this-size-cells', query)
        self.assertEquals(query['this-size-cells'], [0x1])

        self.assertIn('dtb-size', node)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


        node = self.dtbQuery.resolve([{
            'properties': {
                "clock-output-names[*]": ["osc"]
            }
        }])
        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        query = node['query'][0]

        expected = {
            'compatible': ["fixed-clock"],
            '#clock-cells': [0x0],
            'clock-output-names': ["osc"],
            'clock-frequency': [0x337f980],
            'phandle': [0x3],
            'this-node-path': "/clocks/clk-osc",
        }
        self.assertEquals(query, expected)
        self.assertIn('dtb-size', node)
        self.assertEquals(node['dtb-size'], [self.dtbSize])



        node = self.dtbQuery.resolve([{
            'properties': {
                "compatible[*]": ["brcm,bcm2711-pcie"]
            }
        }])

        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        query = node['query'][0]
        expected = {
            'compatible': ["brcm,bcm2711-pcie"],
            'reg': [0xfd500000, 0x9310],
            'device-type': "pci",
            '#address-cells': [0x3],
            '#interrupt-cells': [0x1],
            '#size-cells': [0x2],
            'interrupts': [0x00, 0x94, 0x04, 0x00, 0x94, 0x04],
            'interrupt-names': ["pcie", "msi"],
            'interrupt-map-mask': [0x00, 0x00, 0x00, 0x07],
            'interrupt-map': [0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x8f, 0x04],
            'msi-controller': "",
            'msi-parent': [0x24],
            'ranges': [0x2000000, 0x00, 0xf8000000, 0x06, 0x00, 0x00, 0x4000000],
            'dma-ranges': [0x2000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0000000],
            'brcm,enable-ssc': "",
            'phandle': [0x24],
            'this-address-cells': [0x2],
            'this-size-cells': [0x1],
            'this-node-path': "/scb/pcie@7d500000",
        }
        self.assertEquals(query, expected)
        self.assertIn('dtb-size', node)
        self.assertEquals(node['dtb-size'], [self.dtbSize])

    def test_properties_star_word_and_byte(self):
        node = self.dtbQuery.resolve([{
            'properties': {
                "clocks[*]": [0x06, 0x23, 0x06, 0x30, 0x06, 0x32]
            }
        }])

        self.assertIn('query', node)
        self.assertEquals(len(node['query']), 1)
        query = node['query'][0]
        expected = {
            'compatible': ['brcm,bcm2835-dsi1'],
            'reg': [0xfe700000, 0x8c],
            'interrupts': [0x00, 0x6c, 0x04],
			'#address-cells': [0x01],
			'#size-cells': [0x00],
			'#clock-cells': [0x01],
            'clocks': [0x06, 0x23, 0x06, 0x30, 0x06, 0x32],
            'clock-names': ["phy", "escape", "pixel"],
            'clock-output-names': ["dsi1_byte", "dsi1_ddr2", "dsi1_ddr"],
            'status': "disabled",
            'power-domains':  [0x0b, 0x12],
            'phandle': [0x05],
            'this-address-cells': [0x1],
            'this-size-cells': [0x1],
            'this-node-path': "/soc/dsi@7e700000",
        }
        self.assertEquals(query, expected)
        self.assertIn('dtb-size', node)
        self.assertEquals(node['dtb-size'], [self.dtbSize])


if __name__ == '__main__':
    unittest.main()
