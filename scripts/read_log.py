import numpy as np
import struct, ast
from __future__ import print_function

#TODO: Make this a module
#TODO: provide some command-line conveniences
def save_numpy(target, log, compress=False):
    """Save log dictionary to numpy format

    compress -- Save with compression. Default False
    """
    fn = np.savez_compressed if compress else np.savez
    fn(target, **log)    


def load_as_numpy(filename, filter_pad=True):
    """
    Reads an HPX produced log file, returns a dictionary with numpy data and
    metadata, similar to loading from a .npz file with multiple entries.
    
    If the file does not start with "HPXnpy" (HPX numpy) an exception is raised.

    Options:
    filter_pad -- Remove "--pad" fields from output before returning. Default True.
    """
    fmt = "<BBH"
    with open(filename, "r") as f:
        magic = f.read(6)
        if magic != "HPXnpy": raise IOError(filename + " does not appear to be HPXnp formatted.")
        
        (major, minor, meta_len) = struct.unpack(fmt, f.read(struct.calcsize(fmt)))
        metadata = f.read(meta_len)
        meta = ast.literal_eval(metadata.strip('\x00'))
        data = np.fromfile(f, dtype=meta['descr'])
        
        if filter_pad:
            names = [n for n in data.dtype.names if not n.startswith("--pad")]
            data = data[names]
        
        # Consts dict to numpy record array
        meta = meta.get("consts", {})
        vals = [e[0] for e in meta.values()]
        types = [e[1] for e in meta.values()]        
        consts_arr = np.array([tuple(vals)], dtype=zip(meta.keys(), types))
    return {"data":data, "consts":consts_arr}



def extend_metadata(log):
    """Produce a new log dictionary with standard metadata
    that may have been missing from the original.  Always
    returns a new dictionary (shallow copy of the original).
    """
    
    dtype = log["data"].dtype
    log = log.copy()
    if "mins" not in log:
        log["mins"]  = np.array(tuple([np.min(log["data"][name]) for name in dtype.names]), dtype=dtype)
        
    if "maxes" not in log:
        log["maxes"] = np.array(tuple([np.max(log["data"][name]) for name in dtype.names]), dtype=dtype)
        
    return log


def print_file(filename, **kwargs):
    "Load a log file and print it; kwargs are the same as print_log"
    try:
        log = np.load(filename)
    except:
        log = load_as_numpy(filename)
        
    print_log(log, **kwargs)
        
    
def print_log(log, ensure_widths=False, lines=-1, delimit=" | "):
    """Print the contents of a log dictionary to std out.
    
    Options:
    delimit -- Field delimiter Default is " | ".
    ensure_widths -- Make sure columns widths are wide enough for all 
                     entries (may require a scan of the data to find the widest entry)
                     Default False.
    lines -- Stop after printing the set number of lines (or end of file).
             Default -1 for print all data.
    """

    data = log["data"]
    names = data.dtype.names
    name_lens = [len(str(name)) for name in names]
    
    if ensure_widths and not "maxes" in log or not "mins" in log:
        log = extend_metadata(log)
        
    maxes = [len(str(e)) for e in log["maxes"].tolist()] if "maxes" in log else name_lens
    mins = [len(str(e)) for e in log["mins"].tolist()] if "mins" in log else name_lens

    pads = [max(col) for col in zip(name_lens, maxes, mins)]
    
    format = delimit.join(["{%d: >%d}"%(i,pad) for (i, pad) in enumerate(pads)])
    print(format.format(*names))
        
    for (i, line) in enumerate(data):
        if lines>0 and i > lines: break
        print(format.format(*line))


def print_metadata(log, extend=False):
    """Print the log metadata
    extend -- Run extend_metadata. Default False.
    """
    if extend: log=extend_metadata(log)
    
    names = set(log.keys())
    if "data" in names: 
        data = log["data"]
        names.remove("data")
        print("names: {0}".format(", ".join(data.dtype.names)))
        print("Entry count: {0}".format(len(data)))

    if "consts" in names:
        consts = log["consts"]
        names.remove("consts")
        print("Consts:")
        for name in consts.dtype.names:
            print("   {0} : {1}".format(name, consts[name][0]))
        
    names = sorted(list(names))
    for name in names:
        print("%s : %s" % (name, log[name]))

