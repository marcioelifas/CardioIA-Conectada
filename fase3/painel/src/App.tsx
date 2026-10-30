import { useEffect, useState } from 'react'
import mqtt from 'mqtt'
import { Gauge } from './components/ui/gauge'

// Mesmo formato publicado pelo ESP32 (fase3/wokwi/sketch.ino)
type Vitals = { t: number; temp: number; hum: number; bpm: number; motion: number; alert: number; pending: number }

const BROKER = `ws://${location.hostname}:9001`  // mesmo host do painel: funciona também de outro PC na rede
const TOPIC = 'cardioia/g7f3a/paciente01/vitals'
const HISTORY = 90                                // 90 amostras x 2 s = 3 min no gráfico
const TEMP_MIN = 34, TEMP_MAX = 42

// Mesmos limites do ESP32 e do Node-RED: o painel revalida a regra
function motivos(v: Vitals) {
  const m: string[] = []
  if (v.bpm > 120) m.push(`Taquicardia (${v.bpm} bpm)`)
  if (v.bpm > 0 && v.bpm < 50) m.push(`Bradicardia (${v.bpm} bpm)`)
  if (v.temp > 38) m.push(`Febre (${v.temp.toFixed(1)} °C)`)
  if (v.alert && m.length === 0) m.push('Ausência de movimento')
  return m
}

export default function App() {
  const [hist, setHist] = useState<Vitals[]>([])
  const [conn, setConn] = useState<'conectando' | 'online' | 'offline'>('conectando')

  useEffect(() => {
    const client = mqtt.connect(BROKER, { reconnectPeriod: 3000 })
    client.on('connect', () => { setConn('online'); client.subscribe(TOPIC, { qos: 1 }) })
    client.on('offline', () => setConn('offline'))
    client.on('message', (_t, buf) => {
      try {
        const v = JSON.parse(buf.toString()) as Vitals
        setHist(h => [...h.slice(-(HISTORY - 1)), v])
      } catch { /* mensagem fora do formato: ignora */ }
    })
    return () => { client.end(true) }
  }, [])

  const v = hist[hist.length - 1] as Vitals | undefined
  const alertas = v ? motivos(v) : []
  const emAlerta = alertas.length > 0

  return (
    <main className="min-h-screen bg-slate-950 text-slate-100 p-4 sm:p-6">
      <header className="mx-auto max-w-5xl flex flex-wrap items-center justify-between gap-2 mb-4">
        <div>
          <h1 className="text-xl font-semibold">CardioIA · Paciente 01</h1>
          <p className="text-sm text-slate-400">Monitoramento contínuo · tópico {TOPIC}</p>
        </div>
        <span className="flex items-center gap-2 text-sm text-slate-300">
          <span className={`size-2.5 rounded-full ${conn === 'online' ? 'bg-emerald-400' : conn === 'offline' ? 'bg-red-500' : 'bg-amber-400'}`} />
          Broker {conn}
        </span>
      </header>

      <div className="mx-auto max-w-5xl grid gap-4 sm:grid-cols-3">
        {/* Status / LED de alerta */}
        <section
          aria-live="polite"
          className={`sm:col-span-3 rounded-xl border p-4 flex items-center gap-3 ${emAlerta ? 'border-red-500/60 bg-red-950/60' : 'border-slate-800 bg-slate-900'}`}
        >
          <span className={`size-4 shrink-0 rounded-full ${!v ? 'bg-slate-600' : emAlerta ? 'bg-red-500 motion-safe:animate-pulse' : 'bg-emerald-400'}`} />
          <p className="font-medium">
            {!v ? 'Aguardando dados do dispositivo…' : emAlerta ? `ALERTA: ${alertas.join(', ')}` : 'Sinais vitais dentro da faixa'}
          </p>
        </section>

        {/* Frequência cardíaca */}
        <section className="sm:col-span-2 rounded-xl border border-slate-800 bg-slate-900 p-4">
          <div className="flex items-baseline justify-between">
            <h2 className="text-sm text-slate-400">Frequência cardíaca</h2>
            <p className="text-4xl font-semibold tabular-nums text-emerald-400">{v?.bpm || '--'}<span className="text-base text-slate-400"> bpm</span></p>
          </div>
          <BpmChart data={hist.map(h => h.bpm)} />
        </section>

        {/* Temperatura (Gauge do 21st) */}
        <section className="rounded-xl border border-slate-800 bg-slate-900 p-4 flex flex-col items-center">
          <h2 className="self-start text-sm text-slate-400">Temperatura</h2>
          <div className="relative w-40 mt-2">
            <Gauge
              value={v ? Math.min(100, Math.max(0, ((v.temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN)) * 100)) : 0}
              primary={!v ? '#475569' : v.temp > 38 ? 'danger' : v.temp > 37.5 ? 'warning' : 'success'}
              secondary="#1e293b"
              showValue={false}
              aria-label="Temperatura"
            />
            <p className="absolute inset-0 grid place-items-center text-3xl font-semibold tabular-nums">
              {v ? `${v.temp.toFixed(1)}°` : '--'}
            </p>
          </div>
          <p className="text-xs text-slate-500 mt-1">{TEMP_MIN}–{TEMP_MAX} °C · febre &gt; 38 °C</p>
        </section>

        <Stat label="Movimento" value={!v ? '--' : v.motion ? 'Detectado' : 'Parado'} />
        <Stat label="Umidade" value={v ? `${v.hum}%` : '--'} />
        <Stat label="Sincronização" value={!v ? '--' : v.pending ? `${v.pending} amostras pendentes` : 'Em dia'} />
      </div>
    </main>
  )
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <section className="rounded-xl border border-slate-800 bg-slate-900 p-4">
      <h2 className="text-sm text-slate-400">{label}</h2>
      <p className="text-lg font-medium mt-1">{value}</p>
    </section>
  )
}

// Linha de BPM em SVG puro, com as faixas de alerta (50 e 120 bpm) tracejadas
function BpmChart({ data }: { data: number[] }) {
  const W = 600, H = 160, MAX = 200
  const y = (b: number) => H - (b / MAX) * H
  const pts = data.map((b, i) => `${(i / (HISTORY - 1)) * W},${y(b)}`).join(' ')
  return (
    <svg viewBox={`0 0 ${W} ${H}`} className="w-full h-40 mt-3" role="img" aria-label="Gráfico de frequência cardíaca">
      {[50, 120].map(l => (
        <g key={l}>
          <line x1={0} x2={W} y1={y(l)} y2={y(l)} stroke="#ef4444" strokeOpacity={0.4} strokeDasharray="6 6" />
          <text x={4} y={y(l) - 4} fill="#94a3b8" fontSize={11}>{l}</text>
        </g>
      ))}
      {data.length > 1 && <polyline points={pts} fill="none" stroke="#34d399" strokeWidth={2} strokeLinejoin="round" />}
    </svg>
  )
}
