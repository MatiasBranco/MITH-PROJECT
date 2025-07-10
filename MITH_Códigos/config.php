<?php
header('Content-Type: application/json');
ini_set('display_errors', 1);
error_reporting(E_ALL);

// Define timezone para Lisboa (com horário de verão/inverno automático)
date_default_timezone_set('Europe/Lisbon');

// Inclui conexão com banco de dados
include("ligabd.php");

// Verifica conexão
if ($con->connect_error) {
    http_response_code(500);
    die(json_encode([
        'status' => 'error',
        'message' => 'Erro de conexão: ' . $con->connect_error
    ]));
}

// Verifica método HTTP POST
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    die(json_encode([
        'status' => 'error',
        'message' => 'Método não permitido. Use POST.'
    ]));
}

// Recebe JSON do corpo da requisição
$inputJSON = file_get_contents('php://input');
$input = json_decode($inputJSON, true);

if (json_last_error() !== JSON_ERROR_NONE) {
    http_response_code(400);
    die(json_encode([
        'status' => 'error',
        'message' => 'JSON inválido: ' . json_last_error_msg()
    ]));
}

// Campos obrigatórios
$required = ['temperatura', 'umidade'];
foreach ($required as $field) {
    if (!isset($input[$field])) {
        http_response_code(400);
        die(json_encode([
            'status' => 'error',
            'message' => "Campo obrigatório '$field' faltando."
        ]));
    }
}

// Converte valores para float, substituindo vírgula por ponto (caso venha)
$temperatura = floatval(str_replace(',', '.', $input['temperatura']));
$umidade = floatval(str_replace(',', '.', $input['umidade']));
$localizacao_id = 7; // Id fixo para localizacao

// Valida se são números válidos
if (!is_numeric($temperatura) || !is_numeric($umidade)) {
    http_response_code(400);
    die(json_encode([
        'status' => 'error',
        'message' => 'Temperatura ou Umidade inválidos (não numéricos).'
    ]));
}

// Obtém data/hora atual no formato MySQL
$data_hora_mysql = date('Y-m-d H:i:s');

try {
    // Prepare a query
    $sql = "INSERT INTO leituras (Homid, loc, localizacao_id, tempo) VALUES (?, ?, ?, ?)";
    $stmt = $con->prepare($sql);

    if (!$stmt) {
        throw new Exception("Erro ao preparar query: " . $con->error);
    }

    // Bind dos parâmetros
    // d = double (temperatura), d = double (umidade), i = int (localizacao), s = string (data hora)
    $stmt->bind_param("ddis", $temperatura, $umidade, $localizacao_id, $data_hora_mysql);

    // Executa a query
    if (!$stmt->execute()) {
        throw new Exception("Erro ao executar query: " . $stmt->error);
    }

    // Sucesso - retorna JSON
    echo json_encode([
        'status' => 'success',
        'message' => 'Dados inseridos com sucesso',
        'data' => [
            'temperatura' => $temperatura,
            'umidade' => $umidade,
            'datahora' => $data_hora_mysql
        ]
    ]);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode([
        'status' => 'error',
        'message' => $e->getMessage()
    ]);
} finally {
    if (isset($stmt)) {
        $stmt->close();
    }
    $con->close();
}
