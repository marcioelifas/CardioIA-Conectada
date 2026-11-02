# Roteiro do vídeo (até 4 min)

Sugestão: grave a tela com o notebook aberto no GitHub ou no Jupyter, rolando até cada figura. Tempos aproximados.

**0:00–0:30 · Problema**
"No CardioIA, o ESP32 mede o BPM a cada 2 segundos e hoje alerta com um limite fixo, acima de 120 ou abaixo de 50. A pergunta é: um modelo que olha a **série temporal** detecta melhor? Comparei uma regressão logística com uma rede neuromórfica de neurônios LIF."
→ Mostrar a figura `01-exemplos.png`: normal, taquicardia, bradicardia e crise curta.

**0:30–1:10 · Dados e modelos**
"Os dados são sintéticos, com fisiologia plausível e artefatos de sensor, como um toque duplo no botão. São três versões de regressão logística: com a série crua, com estatísticas genéricas e com atributos clínicos. A rede LIF tem dois neurônios, um para taquicardia e outro para bradicardia. Cada um integra o quanto o BPM sai da faixa e dispara quando passa do limiar."
→ Mostrar `02-lif-membrana.png`: a membrana subindo e os spikes.

**1:10–2:00 · Resultado 1: a representação importa**
"Com atributos genéricos, a regressão logística **não detectou nenhuma bradicardia**. O risco é alto **ou** baixo, e um modelo linear traça uma fronteira só. Quando recebeu a mesma informação clínica que o LIF, ela subiu para F1 de 0,985, e o LIF chegou a 1,0."
→ Mostrar a tabela de resultados e `03-confusao.png`.

**2:00–3:00 · Resultado 2: robustez × latência**
"O ponto mais interessante: o LIF com tau curto alerta em 3 segundos, mas cada artefato também gera um spike. Com muitos artefatos, o F1 dele caiu para 0,78. Re-treinando com aumento de dados, a busca escolheu um tau maior: a robustez voltou para 0,96, mas a detecção passou para 7 segundos. A regressão é robusta, mas em 10% das crises demora 18 segundos."
→ Mostrar `04-robustez.png` e `05-latencia.png`.

**3:00–3:45 · Conclusão crítica**
"O LIF tem duas variáveis de estado e cerca de 10 operações por amostra, e caberia no próprio ESP32, no lugar do limite fixo. A regressão entrega probabilidade e explicação para o médico. Minha recomendação é LIF no Edge e regressão logística na nuvem. A limitação é que os dados são sintéticos; o próximo passo é validar com o PhysioNet e com os dados reais da nossa bancada."

**3:45–4:00 · Fechamento**
"O código, o notebook e o relatório estão no GitHub, com o link na descrição."
