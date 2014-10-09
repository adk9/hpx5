#!/usr/bin/python

import sys
import argparse
import httplib
import json

DPID = "00:02:00:01:e8:8b:1e:32"

node = {}
node['cutter01'] = {'mac': "00:02:c9:24:31:70",
                'mcmac': "33:33:00:01:00:00",
                'port': 2}
node['cutter02'] = {'mac': "f4:52:14:49:50:20",
                'mcmac': "33:33:00:02:00:00",
                'port': 3}
node['cutter03'] = {'mac': "f4:52:14:49:55:40",
                'mcmac': "33:33:00:03:00:00",
                'port': 4}
node['cutter04'] = {'mac': "f4:52:14:49:54:70",
                'mcmac': "33:33:00:04:00:00",
                'port': 5}
node['cutter05'] = {'mac': "00:02:c9:18:07:00",
                'mcmac': "33:33:00:05:00:00",
                'port': 6}
node['cutter06'] = {'mac': "00:02:c9:17:c5:d0",
                'mcmac': "33:33:00:06:00:00",
                'port': 7}
node['cutter07'] = {'mac': "f4:52:14:01:6f:f0",
                'mcmac': "33:33:00:07:00:00",
                'port': 8}
node['cutter08'] = {'mac': "00:02:c9:18:1d:50",
                'mcmac': "33:33:00:08:00:00",
                'port': 9}
node['cutter09'] = {'mac': "00:02:c9:35:03:50",
                'mcmac': "33:33:00:09:00:00",
                'port': 10}
node['cutter10'] = {'mac': "00:02:c9:18:10:40",
                'mcmac': "33:33:00:0a:00:00",
                'port': 11}
node['cutter11'] = {'mac': "f4:52:14:01:71:70",
                'mcmac': "33:33:00:0b:00:00",
                'port': 12}
node['cutter12'] = {'mac': "f4:52:14:49:55:80",
                'mcmac': "33:33:00:0c:00:00",
                'port': 13}
node['cutter13'] = {'mac': "f4:52:14:49:4f:d0",
                'mcmac': "33:33:00:0d:00:00",
                'port': 14}
node['cutter14'] = {'mac': "f4:52:14:49:54:60",
                'mcmac': "33:33:00:0e:00:00",
                'port': 15}
node['cutter15'] = {'mac': "f4:52:14:49:51:a0",
                'mcmac': "33:33:00:0f:00:00",
                'port': 16}
node['cutter16'] = {'mac': "f4:52:14:49:54:40",
                'mcmac': "33:33:00:10:00:00",
                'port': 17}


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
            


