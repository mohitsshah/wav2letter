import logging
import random

from multiprocessing.dummy import Pool
import time

import grpc
import wav2letter_pb2
import wav2letter_pb2_grpc

class recieved_msg:
    recieved_msg=[]
    st_time=[]
    end_time=[]

def make_bstream(bytes_chunk,eos_status,id):
    return wav2letter_pb2.Byte_Stream(bstream=bytes_chunk,eos=eos_status,unique_id=id)

def generate_msg(audio_path):
    with open(audio_path,'rb') as f:
        byte_stream=f.read()
    messages=[]
    n=32100
    id=random.randint(0,10000)
    for i in range(0,len(byte_stream),n):
        if(i+n>=len(byte_stream)):
            messages.append(make_bstream(byte_stream[i:i+n],True,id))
        else:
            messages.append(make_bstream(byte_stream[i:i+n],False,id))

    for msg in messages:
        yield msg

def send_audio(stub,audio_path):
    reply=recieved_msg()
    responses = stub.Search(generate_msg(audio_path))
    for response in responses:
        print(response.start,response.end,response.tstream)
        reply.recieved_msg.append(response.tstream)
        reply.st_time.append(response.start)
        reply.end_time.append(response.end)

    '''with open(str(response.unique_id)+'.wav','wb') as f :
        for i in recieved_msg:
            f.write(i)
    '''

def run(audio_path):
    time.sleep(1)
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = wav2letter_pb2_grpc.echo_bytestreamStub(channel)
        print("client start !!!")
        send_audio(stub,audio_path)

if __name__ == '__main__':
    logging.basicConfig()

    p=Pool()
    files=128*['/Users/mohitshah/Others/asr/test.wav']
    p.map(run,files)
    #for file in files:
     #   run(file)
    #run('test_16k.wav')
