# SonoffAlexaSwitch

Sketch Arduino IDE (não precisa de PlatformIO) para controlar um relé
ESP8266 (Sonoff Basic, por padrão) pela Alexa/Echo Dot, com o mesmo
nível de robustez que o Tasmota buscava: estado persistente, WiFi que
se autorrecupera, configuração sem regravar o firmware, controle local
mesmo sem internet, e atualização OTA.

> Este projeto **não é** uma conversão do fork do Tasmota
> (`tasmota-esp8266-1m-essential`) — esse fork é o firmware Tasmota
> completo, feito para ser compilado só com PlatformIO (o próprio time
> do Tasmota diz que o suporte ao Arduino IDE é "limitado, pois um
> número crescente de recursos não pode ser compilado com o Arduino
> IDE"). Este é um sketch novo, enxuto, com o mesmo objetivo final
> (ligar/desligar pela Echo Dot) e os mesmos princípios de robustez,
> mas compila 100% no Arduino IDE.

## Por que não dava para simplesmente "converter"

O Arduino IDE só compila arquivos `.ino`/`.cpp` que estão **na mesma
pasta** do sketch principal — ele não entra em subpastas. O Tasmota
tem mais de 400 arquivos `.ino` espalhados em `tasmota_support/`,
`tasmota_xdrv_driver/`, `tasmota_xsns_sensor/` etc., então abrir
`tasmota.ino` direto no Arduino IDE ignoraria quase todo o código dos
drivers. Por isso este projeto foi feito do zero, já pensado para
caber inteiro numa única pasta.

## Hardware assumido

- **Sonoff Basic** (ESP8266, 1MB de flash) — mesma placa configurada
  no fork original (`SONOFF_BASIC`, `boards/esp8266_1M.json`).
- Pinos (ajustáveis em `Config.h` se seu módulo for diferente):
  | Função | GPIO |
  |---|---|
  | Relé | GPIO12 (nível alto = ligado) |
  | LED de status | GPIO13 (nível baixo = aceso) |
  | Botão físico | GPIO0 (aterra quando pressionado) |

⚠️ **Segurança**: o Sonoff Basic opera diretamente na tensão da rede
elétrica (110-240V). Só manuseie/energize a placa com os terminais de
alta tensão desconectados, e nunca com a placa ligada à USB ao mesmo
tempo que aos 110/220V.

## Bibliotecas necessárias

No Arduino IDE, adicione o suporte a placas ESP8266 em
**Preferências > URLs Adicionais de Gerenciadores de Placas**:
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```
Depois, em **Ferramentas > Placa > Gerenciador de Placas**, instale
"esp8266 by ESP8266 Community".

Em **Ferramentas > Gerenciar Bibliotecas**, instale:
- **WiFiManager** (por tzapu)

Só essa. A integração com a Alexa **não usa mais o fauxmoESP** (veja o
motivo logo abaixo) — foi reimplementada só com bibliotecas que já vêm
junto do core do ESP8266 (`EEPROM`, `ESP8266WiFi`, `WiFiUdp`,
`ESP8266WebServer`, `ESP8266mDNS`, `ArduinoOTA`), então tem uma
dependência a menos para dar problema.

## Configuração da placa (Ferramentas)

- Placa: **Generic ESP8266 Module**
- Flash Size: **1MB (FS:none, OTA:~502KB)** — mesmo layout do
  `boards/esp8266_1M.json` original (sem sistema de arquivos)
- Reset Method: **ck** (ou "nodemcu" se o `ck` não funcionar com seu
  adaptador USB-serial)
- Upload Speed: 115200

## Primeira configuração (sem digitar senha de WiFi no código)

1. Grave o sketch pela USB (uma única vez).
2. O dispositivo cria uma rede WiFi própria chamada
   `SonoffSetup-XXXXXX`. Conecte seu celular/notebook nela.
3. Uma página de configuração abre automaticamente (portal cativo).
   Escolha sua rede WiFi, defina o **nome do dispositivo** (é o nome
   que a Alexa vai usar) e, se quiser, uma senha para atualização OTA.
4. Salve. O dispositivo reinicia e conecta na sua rede.

Se a rede salva ficar fora do ar depois, o dispositivo tenta
reconectar sozinho e reabre o portal de configuração automaticamente
após alguns minutos sem sucesso — não precisa reflashar.

## Configurando a Alexa

1. Diga **"Alexa, descubra novos dispositivos"** (ou faça isso pelo
   app Alexa) uma vez, com o dispositivo ligado e na mesma rede WiFi
   do Echo Dot.
2. A Alexa encontra o dispositivo pelo nome escolhido no passo
   anterior. Depois é só: **"Alexa, ligar `<nome>`"** /
   **"Alexa, desligar `<nome>`"**.

Isso usa o mesmo mecanismo (emulação de dispositivo Belkin
WeMo/UPnP) que o "Wemo emulation" nativo do Tasmota — sem conta na
nuvem, sem skill. Veja a seção **"Por que a versão anterior falhava
tanto"** abaixo para o motivo de isso ter sido reescrito sem a
biblioteca fauxmoESP.

## Controle local (sem depender da Alexa)

- **Botão físico**: aperto curto liga/desliga o relé na hora, mesmo
  sem WiFi.
- **Aperto longo (5s ou mais)**: apaga a configuração de WiFi e volta
  ao modo de configuração (`SonoffSetup-XXXXXX`) — útil se trocar de
  roteador.
- **Página local**: `http://<hostname>.local/` (o hostname aparece no
  Monitor Serial ao ligar, formato `sonoff-XXXXXX`) — mesma porta 80
  de sempre; a emulação da Alexa roda no mesmo servidor, não precisa
  mais de uma porta separada.
- **API HTTP** simples para automações (Home Assistant, Node-RED,
  atalhos no celular):
  - `GET /api/state` → `{"name":...,"state":true|false,...}`
  - `POST /api/on`
  - `POST /api/off`

## Atualização OTA

Depois da primeira gravação por USB, novas versões podem ser enviadas
pela rede: no Arduino IDE, em **Ferramentas > Porta**, o dispositivo
aparece como uma porta de rede (`sonoff-XXXXXX at 192.168.x.x`). Se
você definiu uma senha OTA no portal de configuração, o IDE vai pedi-la
na hora de gravar.

## O que garante a robustez

- **Estado persistente**: o último estado do relé é salvo (EEPROM
  emulada) e restaurado depois de queda de energia — comportamento
  configurável em `Config.h` (`DEFAULT_POWER_ON_STATE`: sempre
  ligado, sempre desligado, invertido, ou restaurar o último estado).
- **Loop principal 100% não-bloqueante**: nenhuma função usa `delay()`
  bloqueante — WiFi, OTA, servidor web, Alexa e botão são todos
  atendidos a cada ciclo, então um travar não trava os outros.
- **Reconexão automática de WiFi** sem precisar reiniciar o
  dispositivo.
- **Configuração sem hardcode**: nada de senha de WiFi fixa no
  código-fonte.
- **Debounce por software** no botão físico, para não haver cliques
  fantasmas.

## Por que a versão anterior (com fauxmoESP) falhava tanto

A primeira versão deste sketch usava a biblioteca fauxmoESP para a
emulação Wemo. Comparando com a implementação real do Tasmota
(`tasmota_xdrv_driver/xdrv_21_wemo.ino` +
`tasmota_support/support_udp.ino`, que faz a mesma coisa há anos, de
forma confiável), duas diferenças estruturais explicam o
comportamento instável:

1. **fauxmoESP sobe seu próprio servidor HTTP interno**, separado do
   servidor da página local — duas coisas concorrendo por memória e
   sockets no ESP8266, que tem pouquíssima RAM. O Tasmota registra os
   endpoints da emulação Wemo **no mesmo servidor web** que já está
   rodando para tudo mais. Este sketch agora faz o mesmo
   (`WemoEmulation.ino` adiciona suas rotas ao servidor de
   `WebInterface.ino`).

2. **A causa mais provável das falhas "depois de um tempo"**: a
   assinatura no grupo multicast UDP (é assim que a Alexa descobre o
   dispositivo) pode ficar obsoleta depois que o WiFi cai e reconecta
   — o dispositivo continua "ligado" e respondendo na rede local, mas
   some da descoberta da Alexa até um reboot manual. O log do próprio
   Tasmota até tem uma linha só pra isso: `"Multicast (re)joined"` —
   ou seja, o time do Tasmota já bateu nesse mesmo problema e resolveu
   reinscrevendo o grupo multicast toda vez que a rede volta. Este
   sketch agora faz exatamente isso: `WemoNetworkUp()` /
   `WemoNetworkDown()` são chamados a cada mudança de estado do WiFi
   (veja `WifiSetup.ino`), e além disso há uma nova tentativa
   automática em segundo plano caso a inscrição falhe na hora.

Ponto de honestidade: o Tasmota também chama uma função de baixo
nível do lwIP (`igmp_joingroup`) como reforço extra antes de
`WiFiUDP::beginMulticast()`. A assinatura exata dela varia com a
versão do core ESP8266, e este ambiente não tem acesso de rede aos
headers reais do core para confirmar — por isso optei por não
adivinhar essa chamada e ficar só na API pública e estável do
`WiFiUDP`, compensando com o "derrubar e recriar" completo do socket
a cada reconexão (mesmo efeito prático) mais uma nova tentativa
periódica de segurança. Se depois de testar você ainda notar a Alexa
"perdendo" o dispositivo esporadicamente, esse `igmp_joingroup` é o
próximo ajuste a tentar — me avise que eu adiciono.

## Sobre a verificação deste código

Este ambiente não tem acesso de rede ao toolchain real do ESP8266 (os
hosts de download do core/ferramentas estão bloqueados aqui), então
não foi possível compilar com o toolchain oficial do ESP8266 dentro
desta sessão. Em vez disso, o código foi verificado em três frentes:

1. Revisão manual de cada chamada contra o comportamento conhecido de
   cada biblioteca (WiFiManager, ArduinoOTA, ESP8266WebServer,
   ESP8266mDNS, WiFiUdp), e comparação direta com o código-fonte real
   do Tasmota para a parte de emulação Wemo/SSDP.
2. Compilação bem-sucedida, sem nenhum aviso mesmo com
   `-Wall -Wextra -Wswitch-enum -Wformat=2`, contra cabeçalhos
   simulados que reproduzem as assinaturas dessas bibliotecas — o que
   valida toda a lógica própria do sketch (debounce, máquina de
   estados do LED, persistência, fluxo entre módulos). Essa checagem,
   aliás, já pegou um bug real: o buffer da resposta SOAP do
   `/upnp/control/basicevent1` estava pequeno demais e a resposta
   seria truncada — corrigido.
3. As mensagens de rede que o dispositivo realmente envia (a resposta
   HTTP do M-SEARCH/SSDP e a resposta SOAP do `SetBinaryState`) foram
   geradas fora do sketch com os mesmos parâmetros e conferidas byte
   a byte contra o formato exigido pelo protocolo UPnP/Belkin.

Ainda assim, recomendo compilar e testar no Arduino IDE antes de
instalar em definitivo; se algum erro ou aviso aparecer (por exemplo
por causa de uma versão de biblioteca diferente da documentada aqui),
me avise com a mensagem que eu ajusto.
