"""Regras de risco do CardioIA (mesmos limites do ESP32 e do Node-RED)."""

BPM_MAX = 120          # taquicardia
BPM_MIN = 50           # bradicardia (bpm 0 = sem leitura, não conta)
TEMP_MAX = 38.0        # febre
SEM_MOVIMENTO_S = 60   # possível queda ou inconsciência


def avaliar(amostra: dict, ultimo_movimento_t: int) -> list[str]:
    """Devolve a lista de riscos da amostra (vazia = normal).

    ultimo_movimento_t: instante (campo t, em s) da última amostra com movimento.
    """
    riscos = []
    bpm, temp = amostra["bpm"], amostra["temp"]
    if bpm > BPM_MAX:
        riscos.append(f"Taquicardia ({bpm} bpm)")
    if 0 < bpm < BPM_MIN:
        riscos.append(f"Bradicardia ({bpm} bpm)")
    if temp > TEMP_MAX:
        riscos.append(f"Febre ({temp} °C)")
    parado = amostra["t"] - ultimo_movimento_t
    if parado >= SEM_MOVIMENTO_S:
        riscos.append(f"Ausência de movimento ({parado} s)")
    return riscos


if __name__ == "__main__":  # autoteste: python risco.py
    base = {"t": 10, "temp": 36.8, "bpm": 78}
    assert avaliar(base, 10) == []
    assert avaliar({**base, "bpm": 135}, 10) == ["Taquicardia (135 bpm)"]
    assert avaliar({**base, "bpm": 42}, 10) == ["Bradicardia (42 bpm)"]
    assert avaliar({**base, "bpm": 0}, 10) == []
    assert avaliar({**base, "temp": 38.6}, 10) == ["Febre (38.6 °C)"]
    assert avaliar({**base, "t": 75}, 10) == ["Ausência de movimento (65 s)"]
    assert len(avaliar({"t": 100, "temp": 39.0, "bpm": 150}, 0)) == 3
    print("risco.py: ok")
