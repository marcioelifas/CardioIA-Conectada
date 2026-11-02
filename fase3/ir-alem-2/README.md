# Ir Além 2 – IA em séries temporais de frequência cardíaca

Comparação entre **regressão logística** e uma **rede neuromórfica LIF** (2 neurônios) para detectar taquicardia, bradicardia e crises curtas em janelas de 60 s de BPM.

| Arquivo | Conteúdo |
|---|---|
| [`cardioia_lif_vs_logistica.ipynb`](cardioia_lif_vs_logistica.ipynb) | Notebook comentado, já executado (resultados e gráficos visíveis no GitHub) |
| [`../docs/relatorio-ir-alem-2-ia.md`](../docs/relatorio-ir-alem-2-ia.md) | Relatório comparativo |
| [`../docs/evidencias/ir-alem-2/`](../docs/evidencias/ir-alem-2/) | Figuras geradas pelo notebook |
| [`roteiro-video.md`](roteiro-video.md) | Roteiro do vídeo de até 4 minutos |

**Vídeo (YouTube, não listado):** _link a inserir_

## Resultado em uma linha

Com a mesma codificação clínica, LR e LIF empatam em acurácia (F1 ≈ 0,99). O LIF detecta crises em cerca de 3 s com 2 estados e cerca de 10 operações por amostra, e cabe no ESP32. A LR entrega probabilidade calibrada e explicação. O ajuste de τ e k do LIF expõe o equilíbrio entre **latência e robustez a artefatos**.

## Como executar

```bash
cd fase3/ir-alem-2
pip install -r requirements.txt
jupyter notebook cardioia_lif_vs_logistica.ipynb
```

Tudo é gerado no próprio notebook, com semente fixa (`SEED = 42`), sem download de dados. A execução completa leva cerca de 1 minuto.
