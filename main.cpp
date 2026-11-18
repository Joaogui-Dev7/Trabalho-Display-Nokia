#include <iostream>
#include <fstream>
#include <string>
#include <clocale>
#include <cctype>    // usado para completar o nome da porta
#include <windows.h>

#pragma pack(push, 1)//pack(1): alinha de 1 em 1 byte, ou seja, sem buracos. Os campos ficam colados, igual estão no arquivo.

struct CabecalhoArquivo //“isto é um BMP e os pixels começam aqui"
{
    unsigned short assinatura;// cpf=0x4D42
    unsigned int tamanhoArquivo;//o tamanho total do arquivo, em bytes.
    unsigned short reservado1;//reserva 2 bytes
    unsigned short reservado2;//reserva 2 bytes
    unsigned int inicioPixels;//4 bytes: posição (byte) do arquivo onde começam os pixels
};

struct CabecalhoImagem// aqui recebe os dados da imagem
{
    unsigned int tamanhoCabecalho;
    int largura;
    int altura;
    unsigned short planos;
    unsigned short bitsPorPixel;
    unsigned int compressao;
    unsigned int tamanhoImagem;
    int resolucaoHorizontal;
    int resolucaoVertical;
    unsigned int coresUsadas;
    unsigned int coresImportantes;
};

#pragma pack(pop)//pop: devolve o alinhamento guardado. Assim só essas duas structs ficam “coladas”

struct estruturaPixel//bmp guarda o rgb ao contrario bgr
{
    unsigned char azul;
    unsigned char verde;
    unsigned char vermelho;
};


using namespace std;

//comandos padrões
const unsigned char SNC  = 0xD3; // 0xd3=vou te mandar uma imagem
const unsigned char ATX  = 0xD8; //mostra a imagem que está na posição CID, CID Eh a posicao da imagem(0 a 31)
const unsigned char CLR  = 0xC3; // apaga a imagem exibida no display


const unsigned char ACK  = 0x41; // 'A' comando executado, ok
const unsigned char NAK  = 0x4E; // 'N' comando desconhecido ou inválido
const unsigned char MND  = 0x4D; // 'M' pode mandar os dados
const unsigned char NID  = 0x49; // 'I' índice CID fora da faixa / inválido>31
const unsigned char TOUT = 0x44; // timeout na comunicação

const int TOTAL_POSICOES = 32;         // o display tem 32 posições de memória para imagens, índices 0 a 31
const int TAMANHO_IMAGEM = 6 * 84;    // 504 bytes por imagem


//Essa função se trata de uma grande verificação para ver se é possível armazenar a imagem dentro do unsigned char imagem[6][84]
bool consegueAbrirImagem(string nomeArquivo, unsigned char imagem[6][84])
{
    //Verificações da imagem para ver se ela se encaixa nos parâmetros para ser apresentada no display

    ifstream arquivo(nomeArquivo.c_str(), ios::binary); //Abre o arquivo em modo binário

    if (!arquivo.is_open()) //Verifica se o arquivo foi aberto com sucesso
    {
        cout << "Erro ao abrir o arquivo " << nomeArquivo << "!" << endl; // mostra qual arquivo falhou
        return false;
    }

    //variaveis das structs de tamanho da imagem
    CabecalhoArquivo CABarquivo;
    CabecalhoImagem CABimagem;

    arquivo.read(reinterpret_cast<char*>(&CABarquivo), sizeof(CABarquivo)); //Lê o cabeçalho do arquivo -- o reinterpret_cast é usado para converter o ponteiro do tipo CabecalhoArquivo* (struct) para do tipo char*

    arquivo.read(reinterpret_cast<char*>(&CABimagem), sizeof(CABimagem)); //Lê o cabeçalho da imagem

    // Se o arquivo for menor que os dois cabeçalhos, a leitura falha e as structs ficam com lixo.
    // Sem esse teste, as verificações abaixo comparariam valores aleatórios.
    if (!arquivo)
    {
        cout << nomeArquivo << ": arquivo muito pequeno ou corrompido!" << endl;
        return false;
    }

    if(CABarquivo.assinatura != 0x4D42) //Verifica se o arquivo é um BMP
    {
        cout << "O arquivo não é um BMP!" << endl;
        return false;
    }

    if(CABimagem.largura != 84 || (CABimagem.altura != 48 && CABimagem.altura != -48)) //Verifica se o arquivo está do tamanho correto
    {
        cout << "Imagem não é do tamanho esperado" << endl;
        return false;
    }

    if (CABimagem.bitsPorPixel != 24) // Verifica se a imagem possui 24 bits por pixel.
    {
        cout << "A imagem precisa ser BMP de 24 bits!" << endl;
        return false;
    }

    if (CABimagem.compressao != 0) // Verifica se a imagem nao possui compressao.
    {
        cout << "A imagem precisa estar sem compressao!" << endl;
        return false;
    }

    //Para fins de teste começar com todos os pixels do display desligados
    for(int i = 0; i < 6; i++)
    {
        for(int u = 0; u < 84; u ++)
        {
            imagem[i][u] = 0; //passa por todos os bits igualando eles a 0 (desligando eles)
        }
    }

    //  Cálculo do padding, iamgens BMP cada linha precisa ter um número de bytes múltiplo de 4.
    // O arquivo completa a linha com bytes de enchimento se precisar.
    int bytesPorLinha = ((84 * 3 + 3) / 4) * 4;
    int padding = bytesPorLinha - 84 * 3;


    //procura o local onde o pixel da imagem realmente se inicia -- ios::beg serve para posicionar o cursor de leitura do arquivo no início dos dados dos pixels.
    arquivo.seekg(CABarquivo.inicioPixels, ios::beg);

    //percorre as linhas
    for(int linhaInicioImagem = 0; linhaInicioImagem < 48; linhaInicioImagem++)
    {
        //Verifica se a imagem vai ter altura positiva(de cabeça para cima) ou negativa(de cabeça para baixo)
        int linhaInicioReal;

        if(CABimagem.altura > 0)
        {
            linhaInicioReal = 47 - linhaInicioImagem;
        }
        else
        {
            linhaInicioReal = linhaInicioImagem;
        }

        for(int x = 0; x < 84; x++)
        {
            estruturaPixel Pixel;

            arquivo.read(reinterpret_cast<char*>(&Pixel), sizeof(estruturaPixel));

            //  Se o arquivo acabar antes dos 48 x 84 pixels, para aqui em vez de usar lixo.
            if (!arquivo)
            {
                cout << nomeArquivo << ": erro ao ler os pixels (arquivo incompleto)!" << endl;
                return false;


            }

            //teste para ver o "brilho" do pixel, caso seu brilho for menor do que 128 ele será considerado desligado
            int brilho = ((Pixel.vermelho * 30 + Pixel.verde * 59 + Pixel.azul * 11) / 100); //calculo para descobrir o "brilho"

            // Pixel escuro (brilho < 128) = ponto ligado (preto) no LCD.
            if(brilho < 128)
            {
                imagem[linhaInicioReal / 8][x] |= (1 << (linhaInicioReal % 8));
            }
        }

        // Pula os bytes de enchimento do final da linha (zero neste caso, ver cálculo acima).
        arquivo.seekg(padding, ios::cur);
    }

    arquivo.close();

    cout << "Imagem " << nomeArquivo << " carregada com sucesso!!!" << endl; // inclui o nome do arquivo

    return true;
}

//
HANDLE abrirPortaSerial(string nomePorta, DWORD baudrate) //DWORD baudrate: a velocidade da comunicação
{
    HANDLE porta; //Cria a variável que vai guardar o ticket(entrada saida) quando a porta for aberta(HANDLE)

    DCB configuracao = {}; //struct do Windows com todas as configurações da porta (DCB)
    COMMTIMEOUTS tempos = {}; //timeouts

    // Portas COM10 em diante só abrem com o prefixo "\\.\" (ex.: "\\.\COM12").
    // O prefixo também funciona para COM1 a COM9, então usamos sempre.
    string caminhoPorta = "\\\\.\\" + nomePorta;

    porta = CreateFileA(caminhoPorta.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); //abre a porta serial de comunicação

    if (porta == INVALID_HANDLE_VALUE)
    {
        cout << "Erro ao abrir a porta serial!" << endl;
        return INVALID_HANDLE_VALUE;//ticket inválido
    }

    configuracao.DCBlength = sizeof(DCB);//O Windows exige que você diga o tamanho da struct DCB antes de usá-la.

    if (!GetCommState(porta, &configuracao)) //pega o estado da variavel configuração e ve se da pra abrir
    {
        cout << "Erro ao obter a configuracao da porta!" << endl;
        CloseHandle(porta);
        return INVALID_HANDLE_VALUE;
    }


    configuracao.BaudRate = baudrate;
    configuracao.ByteSize = 8;
    configuracao.Parity = EVENPARITY;  //a atividade pede paridade PAR
    configuracao.fParity = TRUE;       //liga a verificação de paridade
    configuracao.StopBits = ONESTOPBIT;

    // Desliga controle de fluxo (handshake)é como um semaforo. O GetCommState devolve o que estava configurado antes;
    // se algum programa tivesse deixado o handshake ligado, a escrita poderia travar esperando sinais que o kit não manda.
    // desativa pq o arm n usa isso
    configuracao.fOutxCtsFlow = FALSE;
    configuracao.fOutxDsrFlow = FALSE;
    configuracao.fOutX = FALSE;
    configuracao.fInX = FALSE;
    configuracao.fDtrControl = DTR_CONTROL_ENABLE;
    configuracao.fRtsControl = RTS_CONTROL_ENABLE;

    //ve se é possivel setar as configurações
    if (!SetCommState(porta, &configuracao))
    {
        cout << "Erro ao configurar a porta serial!" << endl;
        CloseHandle(porta);
        return INVALID_HANDLE_VALUE;
    }

    //a leitura espera até 1 segundo antes de desistir.
    tempos.ReadIntervalTimeout = 0;
    tempos.ReadTotalTimeoutMultiplier = 0;
    tempos.ReadTotalTimeoutConstant = 1000;   // espera até 1000 ms por uma resposta
    tempos.WriteTotalTimeoutMultiplier = 0;
    tempos.WriteTotalTimeoutConstant = 1000;  // escrita também desiste após 1000 ms

    if (!SetCommTimeouts(porta, &tempos))
    {
        cout << "Erro ao configurar os timeouts!" << endl;
        CloseHandle(porta);
        return INVALID_HANDLE_VALUE;
    }

    //  Joga fora qualquer byte velho que esteja no buffer da porta antes de começar.
    PurgeComm(porta, PURGE_RXCLEAR | PURGE_TXCLEAR);

    cout << "Porta serial " << nomePorta << " aberta!" << endl;

    return porta;
}

bool enviarDadosImagem(HANDLE porta, unsigned char imagem[6][84])//manda os 504 bytes da imagem para o display pela seria
{
    DWORD bytesEnviados = 0;

    if (!WriteFile(porta, &imagem[0][0], TAMANHO_IMAGEM, &bytesEnviados, NULL))
    {
        cout << "Erro ao enviar a imagem!" << endl;
        return false;
    }

    if (bytesEnviados != TAMANHO_IMAGEM)
    {
        cout << "Nem todos os bytes foram enviados!" << endl;
        return false;
    }

    return true;
}

//Envia um comando de 2 bytes (todo comando do protocolo tem no mínimo 2 bytes). ex: SNC + posição: “vou mandar uma imagem para a posição X”
bool enviarComando(HANDLE porta, unsigned char byte1, unsigned char byte2)
{
    unsigned char comando[2] = { byte1, byte2 };
    DWORD bytesEnviados = 0;

    if (!WriteFile(porta, comando, 2, &bytesEnviados, NULL) || bytesEnviados != 2)
    {
        cout << "Erro ao enviar o comando!" << endl;
        return false;
    }

    return true;
}

//transforma o byte de resposta do display em um texto legível, para as mensagens de erro.
string descreverResposta(unsigned char resposta)
{
    switch (resposta)
    {
        case ACK:  return "ACK (comando executado)";
        case NAK:  return "NAK (comando desconhecido ou invalido)";
        case MND:  return "MND (pode mandar os dados)";
        case NID:  return "NID (indice fora da faixa)";
        case TOUT: return "TOUT (timeout no display)";
        default:   return "resposta desconhecida";
    }
}

// Lê 1 byte de resposta e confere se é o esperado.
// Retorna false se nada chegar dentro do timeout (1 s) ou se chegar outra resposta.
bool esperarResposta(HANDLE porta, unsigned char esperado)
{
    unsigned char resposta = 0;
    DWORD bytesLidos = 0;

    if (!ReadFile(porta, &resposta, 1, &bytesLidos, NULL) || bytesLidos != 1)
    {
        cout << "O display nao respondeu (timeout)!" << endl;
        return false;
    }

    if (resposta != esperado)
    {
        cout << "Resposta inesperada: 0x" << hex << (int)resposta << dec
             << " - " << descreverResposta(resposta)
             << " (esperava " << descreverResposta(esperado) << ")" << endl;
        return false;//ex: Resposta inesperada: 0x49 - NID (indice fora da faixa) (esperava MND (pode mandar os dados))
    }

    return true;
}

//\ Sequência completa do comando SNC, conforme o enunciado:
//   PC envia SNC + CID  ->  display responde MND
//   PC envia 504 bytes  ->  display responde ACK
bool enviarImagemParaPosicao(HANDLE porta, int indice, unsigned char imagem[6][84])
{
    if (indice < 0 || indice >= TOTAL_POSICOES)//0 a 31
    {
        cout << "Indice " << indice << " invalido (precisa ser de 0 a 31)!" << endl;
        return false;
    }

    PurgeComm(porta, PURGE_RXCLEAR); // descarta respostas velhas para não confundir com a nova

    // CID = índice com o bit mais significativo ligado (0 -> 0x80, 1 -> 0x81, ... 31 -> 0x9F)
    if (!enviarComando(porta, SNC, (unsigned char)(0x80 | indice)))
        return false;

    if (!esperarResposta(porta, MND)) // display avisa que está pronto para receber os 504 bytes
        return false;

    if (!enviarDadosImagem(porta, imagem))
        return false;

    if (!esperarResposta(porta, ACK)) // display confirma que guardou a imagem
        return false;

    return true;
}

// Comando ATX: mostra no display a imagem guardada na posição CID.
bool exibirPosicao(HANDLE porta, int indice)//da o comando pra exibir a imagem
{
    PurgeComm(porta, PURGE_RXCLEAR);

    if (!enviarComando(porta, ATX, (unsigned char)(0x80 | indice)))
        return false;

    return esperarResposta(porta, ACK);
}

//  Comando CLR: apaga a imagem exibida no display (pelo enunciado, o byte CLR é enviado duas vezes).
bool apagarDisplay(HANDLE porta)
{
    PurgeComm(porta, PURGE_RXCLEAR);

    if (!enviarComando(porta, CLR, CLR))
        return false;

    return esperarResposta(porta, ACK);
}

//  Lê imagem0.bmp até imagem31.bmp e envia cada uma para a posição de mesmo número.
//  uma imagem que falta ou está errada NÃO aborta tudo:
// ela é pulada e marcada como não carregada no vetor "carregadas".
// Retorna quantas imagens foram enviadas com sucesso.
int carregarAlbum(HANDLE porta, bool carregadas[TOTAL_POSICOES])
{
    unsigned char imagem[6][84] = {}; //Local onde será armazenada a imagem após rodar toda a função consegueAbrirImagem
    int quantidade = 0;

    for (int i = 0; i < TOTAL_POSICOES; i++)
    {
        carregadas[i] = false;

        string nomeArquivo = "imagem" + to_string(i) + ".bmp";

        if (!consegueAbrirImagem(nomeArquivo, imagem))
        {
            cout << "  -> posicao " << i << " ficara vazia." << endl;
            continue; // pula para a próxima imagem
        }

        if (enviarImagemParaPosicao(porta, i, imagem))
        {
            cout << "  -> enviada para a posicao " << i << "." << endl;
            carregadas[i] = true;
            quantidade++;
        }
        else
        {
            cout << "  -> falha ao enviar para a posicao " << i << "." << endl;
        }
    }

    return quantidade;
}

//  Regra de exibição do álbum: cada Enter mostra a próxima imagem carregada, em loop.
// Digitar S e Enter encerra.
void exibirAlbum(HANDLE porta, bool carregadas[TOTAL_POSICOES])
{
    int indice = 0;

    cout << endl << "=== Album === (Enter: proxima imagem | S + Enter: sair)" << endl;

    while (true)
    {
        // procura a próxima imagem que foi carregada (pula as posições vazias)
        while (!carregadas[indice])
        {
            indice = (indice + 1) % TOTAL_POSICOES;
        }

        if (exibirPosicao(porta, indice))
        {
            cout << "Exibindo imagem " << indice << endl;
        }
        else
        {
            cout << "Nao foi possivel exibir a imagem " << indice << endl;
        }

        string entrada;

        if (!getline(cin, entrada)) // se a entrada do teclado for fechada (Ctrl+Z), sai do laço
        {
            break;
        }

        if (entrada == "s" || entrada == "S")
        {
            break;
        }

        indice = (indice + 1) % TOTAL_POSICOES; // % faz voltar para 0 depois do 31
    }
}

int main()
{
    setlocale(LC_ALL, "portuguese");

    //  o programa procura imagem0.bmp ... imagem31.bmp.
    // Agora o usuário digita só a porta serial.
    string nomePorta;

    cout << "Digite a porta serial: ";
    getline(cin, nomePorta); // getline em vez de cin >> para não sobrar o Enter no buffer (atrapalharia o álbum)

    // se o usuário digitar só o número completa com o COM
    if (!nomePorta.empty() && isdigit((unsigned char)nomePorta[0]))
    {
        nomePorta = "COM" + nomePorta;
    }

    HANDLE porta = abrirPortaSerial(nomePorta, 115200); // 115200 bps

    if (porta == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    bool carregadas[TOTAL_POSICOES];

    int quantidade = carregarAlbum(porta, carregadas);

    cout << endl << quantidade << " de " << TOTAL_POSICOES << " imagens enviadas ao display." << endl;

    if (quantidade == 0)
    {
        cout << "Nenhuma imagem foi carregada. Verifique os arquivos e a conexao." << endl;
        CloseHandle(porta);
        return 1;
    }

    cout << "Album pronto para ser exibido no display!" << endl; //  as imagens já foram enviadas, falta só exibir

    exibirAlbum(porta, carregadas);

    apagarDisplay(porta); // limpa o display ao sair

    CloseHandle(porta); //  fecha a porta para liberá-la para outros programas

    return 0;
}