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
NUM_BYTES = 16384

def generate_msg(stub, audio_path):
    def thread_function(stream):
        while True:
            response = next(stream)
            if response:
                print (response.uid, response.seq, response.finished, response.text)
                if response.finished:
                    break
            else:
                break

    send_queue = queue.Queue()
    decode_stream = stub.DecodeStream(iter(send_queue.get, None))
    thread = threading.Thread(target=thread_function, args=(decode_stream,))
    thread.daemon = True
    thread.start()

    with open(audio_path,'rb') as f:
        audio = f.read()
    messages = []
    uid = str(random.randint(0,10000))
    seq = 0
    index = 0
    while True:
        chunk = audio[index:index+NUM_BYTES]
        payload = wav2letter_pb2.DecodeInput(uid=uid, seq=seq, finished=False, audio=chunk)
        send_queue.put(payload)
        if len(chunk) < NUM_BYTES:
            break
        index += NUM_BYTES
        seq += 1

    send_queue.put(None)

    thread.join()

def send_audio(stub,audio_path):
    generate_msg(stub, audio_path)

def run(audio_path):
    with grpc.insecure_channel(SERVER_URL) as channel:
        stub = wav2letter_pb2_grpc.TranscriberStub(channel)
        logging.info("client start !!!")
        send_audio(stub,audio_path)

if __name__ == '__main__':
    files = ['/Users/mohitshah/Others/asr/test20.wav']
    for file in files:
       run(file)
    # p=Pool()
    # p.map(run,files)
    # run('test_16k.wav')

