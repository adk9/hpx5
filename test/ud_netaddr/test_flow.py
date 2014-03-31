#!/usr/bin/python

import sys
import argparse
import httplib
import json

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

def make_json_flow(hop, srcm, dstm, inp, outp, vlan, i):
    return {'switch': switches[hop]['dpid'],
            'name': 'flow-mod-'+str(i),
#            'src-mac': srcm,
#            'dst-mac': dstm,
            'ingress-port': inp,
            'vlan-id': vlan,
            'active': 'true',
            'actions': 'output='+str(outp)
            }

usage_desc = """
nddi_paths.py {add|del} src dst [vlan] ...
"""

parser = argparse.ArgumentParser(description='process args', usage=usage_desc, epilog='foo bar help')
parser.add_argument('--ip', default='localhost')
parser.add_argument('--port', default=8080)
parser.add_argument('cmd')
parser.add_argument('src')
parser.add_argument('dst')
parser.add_argument('vlan', nargs='?')
parser.add_argument('otherargs', nargs='*')
args = parser.parse_args()

#print "Called with:", args
cmd = args.cmd

# handle to Floodlight REST API
rest = RestApi(args.ip, args.port)

flows = [
    {'switch': '00:02:00:01:e8:8b:1e:32',
     'name': 'flow-mod-01',
     'active': 'true',
     'vlan-id': 0,
     'dst-mac': "00:02:de:ad:be:ef",
     'actions': 'set-dst-mac=00:02:c9:18:10:40,output=17'
     },
    {'switch': '00:02:00:01:e8:8b:1e:32',
     'name': 'flow-mod-02',
     'active': 'true',
     'vlan-id': 0,
     'dst-mac': "00:02:ca:fe:ba:be",
     'actions': 'set-dst-mac=00:02:c9:18:1d:50,output=16'
     },
    {'switch': '00:02:00:01:e8:8b:1e:32',
     'name': 'flow-mod-03',
     'active': 'true',
     'vlan-id': 0,
     'dst-mac': "00:02:c9:18:1d:50",
     'actions': 'output=16'
     },
    {'switch': '00:02:00:01:e8:8b:1e:32',
     'name': 'flow-mod-04',
     'active': 'true',
     'vlan-id': 0,
     'dst-mac': "00:02:c9:18:10:40",
     'actions': 'output=17'
     }
    ]

for f in flows:
    if (cmd=='add'):
        print "Adding flow:", f
        rest.set(f)
    if (cmd=='del'):
        print "Deleting flow:", f
        rest.remove(f)
            


