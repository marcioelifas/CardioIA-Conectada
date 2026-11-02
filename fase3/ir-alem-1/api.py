"""API REST do CardioIA: recebe e devolve sinais vitais e avalia o risco de cada amostra.

    uvicorn api:app --port 8000      -> documentação interativa em http://localhost:8000/docs
"""
from collections import defaultdict, deque
from datetime import datetime

from fastapi import FastAPI
from pydantic import BaseModel

import risco

app = FastAPI(title="CardioIA - API de sinais vitais")

# ponytail: histórico em memória (últimas 1000 amostras por paciente); em produção, banco de séries temporais.
historico: dict[str, deque] = defaultdict(lambda: deque(maxlen=1000))
ultimo_movimento: dict[str, int] = {}


class Vitals(BaseModel):
    """Mesmo JSON que o ESP32 publica no MQTT."""
    t: int            # segundos desde o boot do dispositivo
    temp: float
    hum: float
    bpm: int
    motion: int
    alert: int = 0    # alerta já calculado no Edge
    pending: int = 0


@app.post("/pacientes/{paciente}/vitals")
def receber(paciente: str, v: Vitals):
    amostra = v.model_dump()
    # Dispositivo reiniciou (t voltou) ou houve movimento: zera o relógio de imobilidade.
    if v.motion or v.t < ultimo_movimento.get(paciente, -1):
        ultimo_movimento[paciente] = v.t
    ultimo_movimento.setdefault(paciente, v.t)
    riscos = risco.avaliar(amostra, ultimo_movimento[paciente])
    registro = {"recebido_em": datetime.now().isoformat(timespec="seconds"), **amostra, "riscos": riscos}
    historico[paciente].append(registro)
    return {"paciente": paciente, "risco": bool(riscos), "riscos": riscos}


@app.get("/pacientes/{paciente}/vitals")
def listar(paciente: str, limite: int = 20):
    return list(historico[paciente])[-limite:]


@app.get("/pacientes/{paciente}/alertas")
def alertas(paciente: str, limite: int = 20):
    return [r for r in historico[paciente] if r["riscos"]][-limite:]
