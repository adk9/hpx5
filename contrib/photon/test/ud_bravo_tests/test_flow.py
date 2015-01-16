#!/usr/bin/python

import sys
import argparse
import httplib
import json

DPID = "00:02:00:01:e8:8b:1e:32"

node = {}
node['b001'] = {'mac': "00:02:C9:4B:1C:9C",
                'ibmac': "c9:ff:fe:4b:1c:9c",
                'mcmac': "33:33:00:01:00:00",
                'port': 12}
node['b002'] = {'mac': "00:02:C9:35:03:50",
                'ibmac': "c9:ff:fe:35:03:50",
                'mcmac': "33:33:00:02:00:00",
                'port': 13}
node['b003'] = {'mac': "00:02:C9:17:C5:D0",
                'ibmac': "c9:ff:fe:17:c5:d0",
                'mcmac': "33:33:00:03:00:00",
                'port': 14}
node['b004'] = {'mac': "00:02:C9:18:07:00",
                'ibmac': "c9:ff:fe:18:07:00",
                'mcmac': "33:33:00:04:00:00",
                'port': 15}
node['b005'] = {'mac': "00:02:C9:18:1D:50",
                'ibmac': "c9:ff:fe:18:1d:50",
                'mcmac': "33:33:00:05:00:00",
                'port': 16}
node['b006'] = {'mac': "00:02:C9:18:10:40",
                'ibmac': "c9:ff:fe:18:10:40",
                'mcmac': "33:33:00:06:00:00",
                'port': 17}
node['b007'] = {'mac': "F4:52:14:01:71:70",
                'ibmac': "c9:ff:fe:01:71:70",
                'mcmac': "33:33:00:07:00:00",
                'port': 20}
node['b008'] = {'mac': "F4:52:14:01:6F:F0",
                'ibmac': "c9:ff:fe:01:6f:f0",
                'mcmac': "33:33:00:08:00:00",
                'port': 19}


class RestApi(object):

    def __init__(self, server, port):
        self.server = server
        self.port = port


    def get(self, data):
        ret = self.rest_call({}, 'GET')
        return json.loads(ret[2])

    def set(self, data):
        ret = self.rest_call(data, 'POST')
        return ret[0] == 200

    def remove(self, data):
        ret = self.rest_call(data, 'DELETE')
        return ret[0] == 200

    def rest_call(self, data, action):
        path = '/wm/staticflowentrypusher/json'
        headers = {
            'Content-type': 'application/json',
            'Accept': 'application/json',
            }
        body = json.dumps(data)
        conn = httplib.HTTPConnection(self.server, self.port)
        conn.request(action, path, body, headers)
        response = conn.getresponse()
        ret = (response.status, response.reason, response.read())
        print ret
        conn.close()
        return ret

def make_direct_flow(entry, name, vlan):
    return {'switch': DPID,
            'name': 'direct-'+str(name),
            'active': 'true',
            'vlan-id': vlan,
            'dst-mac': entry['mac'],
            'actions': 'output='+str(entry['port'])
            }

def make_rewrite_flow(entry, name, vlan):
    return {'switch': DPID,
            'name': 'mod-'+str(name),
            'active': 'true',
            'vlan-id': vlan,
            'dst-mac': entry['mcmac'],
            #'actions': 'set-dst-mac='+str(entry['ibmac'])+',output='+str(entry['port'])
            'actions': 'output='+str(entry['port'])
            }

usage_desc = """
test_flow.py {add|del} [vlan] ...
"""

parser = argparse.ArgumentParser(description='process args', usage=usage_desc, epilog='foo bar help')
parser.add_argument('--ip', default='localhost')
parser.add_argument('--port', default=8080)
parser.add_argument('cmd')
parser.add_argument('vlan', nargs='?', default=0)
parser.add_argument('otherargs', nargs='*')
args = parser.parse_args()

#print "Called with:", args
cmd = args.cmd

# handle to Floodlight REST API
rest = RestApi(args.ip, args.port)

flows = []
for key in node:
    flows.append(make_direct_flow(node[key], key, args.vlan))
    flows.append(make_rewrite_flow(node[key], key, args.vlan))

for f in flows:
    if (cmd=='add'):
        print "Adding flow:", f
        rest.set(f)
    if (cmd=='del'):
        print "Deleting flow:", f
        rest.remove(f)
            


