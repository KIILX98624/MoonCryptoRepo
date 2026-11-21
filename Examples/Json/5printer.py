import os
import json
import time

os.system("cls")
time.sleep(1)

with open("4conversor.json", "r", encoding="utf-8") as arquivo:
    dados = json.load(arquivo)

print(json.dumps(dados, indent=4, ensure_ascii=False))

asd = input()

os.system("cls")