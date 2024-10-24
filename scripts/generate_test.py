import random

SPECIAL_OP_PROB = 0.04

ops = ["B", "S"]
instruments = [
    "AAPL", "GOOG", "MSFT", "AMZN", "TSLA",
    "FB", "NVDA", "INTC", "CSCO", "ADBE",
    "PYPL", "NFLX", "CMCSA", "PEP", "TMO",
    "AVGO", "TXN", "QCOM", "COST", "TMUS"
    "BIN", "BAC", "WFC", "JPM", "GS",
    "MS", "C", "USB", "PNC", "BK"
    "BINARYD", "BINARYC", "BINARYB", "BINARYA", "BINARYE",
]

id = 0

def generate(
    num_clients: int, num_transactions: int, num_instruments: int,
    filename: str, has_cancel: bool = True, use_round_numbers: bool = False
):
    global id

    if num_instruments == -1:
        instrument_list = instruments
    else:
        instrument_list = instruments[:num_instruments]

    if num_clients < 1:
        print("Invalid number of clients")
        return 

    round_numbers = list(range(10, 101, 10))

    if num_clients == 1:
        print("Single client")
        with open(filename, "w") as f:
            f.write("1\n")
            f.write("o\n")

            for i in range(num_transactions):
                prob = random.random()
                if has_cancel and id > 0 and prob < 5 * SPECIAL_OP_PROB:
                    f.write(f"C {random.randint(0, id-1)}\n")
                else:
                    op = random.choice(ops)
                    instrument = random.choice(instrument_list)
                    count = random.choice(round_numbers) if use_round_numbers else random.randint(1, 100)
                    price = random.choice(round_numbers) if use_round_numbers else random.randint(1, 100)
                    f.write(f"{op} {id} {instrument} {count}\n")
                    id += 1
            f.write("x\n")
    
    else:

        if has_cancel:
            client_id_lookup = {} # map operation id to client id for cancel 

        print(f"{num_clients} clients")
        with open(filename, "w") as f:
            f.write(f"{num_clients}\n")
            f.write(f"0-{num_clients-1} o\n") # connect all threads to server

            for i in range(num_transactions):
                prob = random.random()

                num_sync = random.randint(2, num_clients)
                multi_clients = random.sample(range(num_clients), num_sync)
                multi_clients = ",".join([str(c) for c in multi_clients])

                single_client = random.randint(0, num_clients-1)

                if prob < SPECIAL_OP_PROB: # sleep
                    if random.random() < 0.5:
                        f.write(f"{multi_clients} s {random.randint(0, 1000)}\n") 
                    else:
                        f.write(f"{single_client} s {random.randint(0, 1000)}\n")
                elif SPECIAL_OP_PROB <= prob < 2 * SPECIAL_OP_PROB: # sync
                    f.write(f"{multi_clients} .\n")
                elif id > 0 and 2 * SPECIAL_OP_PROB <= prob < 3 * SPECIAL_OP_PROB: # wait for active order to be matched
                    if random.random() < 0.5:
                        f.write(f"{multi_clients} w {random.randint(0, id-1)}\n")
                    else:
                        f.write(f"{single_client} w {random.randint(0, id-1)}\n")
                elif has_cancel and id > 0 and 3 * SPECIAL_OP_PROB <= prob < 4 * SPECIAL_OP_PROB: # cancel   
                    op_id = random.randint(0, id-1)
                    f.write(f"{client_id_lookup[op_id]} C {op_id}\n")
                else: # buy or sell
                    op = random.choice(ops)
                    instrument = random.choice(instrument_list)
                    count = random.choice(round_numbers) if use_round_numbers else random.randint(1, 100)
                    price = random.choice(round_numbers) if use_round_numbers else random.randint(1, 100)
                    f.write(f"{single_client} {op} {id} {instrument} {price} {count}\n")
                    if has_cancel:
                        client_id_lookup[id] = single_client
                    id += 1

            f.write(f"0-{num_clients-1} x\n") # disconnect all clients

        
if __name__=="__main__":
    num_clients = int(input("Number of clients: ") or 1)
    num_transactions = int(input("Number of test transactions: ") or 100)
    num_instruments = int(input(f"Number of instruments [1-{len(instruments)}]: ") or 1)
    filename = input("Name of the test file: ") or "test"
    filename += ".in"
    has_cancel = input("Include cancel operations? (Y/n) ") or "y"
    has_cancel = has_cancel.lower() == "y"
    use_round_numbers = input("Use round numbers for price and count? (Y/n) ") or "y"
    use_round_numbers = use_round_numbers.lower() == "y"
    generate(num_clients, num_transactions, num_instruments, filename, has_cancel, use_round_numbers)
