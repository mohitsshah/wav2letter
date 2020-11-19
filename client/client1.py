import logging
import random

from multiprocessing.dummy import Pool
import time

import grpc
import wav2letter_pb2
import wav2letter_pb2_grpc
import queue
import threading

logging.basicConfig(level=logging.INFO)

SERVER_URL = 'localhost:50051'
class recieved_msg:
    recieved_msg=[]
    st_time=[]
    end_time=[]

def make_bstream(bytes_chunk,eos_status,id):
    return wav2letter_pb2.Byte_Stream(bstream=bytes_chunk,eos=eos_status,unique_id=id)

def generate_msg(stub, audio_path):
    def thread_function(stream):
        while True:
            response = next(stream)
            if response:
                print (response.start, response.end, response.tstream)
            else:
                break

    send_queue = queue.Queue()
    my_event_stream = stub.Search(iter(send_queue.get, None))
    with open(audio_path,'rb') as f:
        byte_stream=f.read()
    messages=[]
    n=32044
    id=random.randint(0,10000)
    for i in range(0,len(byte_stream),n):
        if(i+n>=len(byte_stream)):
            message = make_bstream(byte_stream[i:i+n],True,id)
        else:
            message = make_bstream(byte_stream[i:i+n],False,id)
        send_queue.put(message)
    send_queue.put(None)
    thread = threading.Thread(target=thread_function, args=(my_event_stream,))
    thread.daemon = True
    thread.start()
    thread.join()
    # while 1:
    #     time.sleep(1)
    # for msg in messages:
    #     yield msg

def send_audio(stub,audio_path):
    generate_msg(stub, audio_path)
    # reply=recieved_msg()
    # responses = stub.Search(generate_msg(audio_path))
    # for response in responses:
    #     logging.info('{} {} {}'.format(response.start,response.end,response.tstream))
    #     reply.recieved_msg.append(response.tstream)
    #     reply.st_time.append(response.start)
    #     reply.end_time.append(response.end)

def run(audio_path):
    with grpc.insecure_channel(SERVER_URL) as channel:
        stub = wav2letter_pb2_grpc.echo_bytestreamStub(channel)
        logging.info("client start !!!")
        send_audio(stub,audio_path)

if __name__ == '__main__':
    # p=Pool()
    files = ['/Users/mohitshah/Others/asr/test.wav']
    # p.map(run,files)
    for file in files:
       run(file)
    #run('test_16k.wav')
