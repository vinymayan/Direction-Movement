#include "Settings.h"
#include "Events.h"
#include "InputManagerAPI.h"
namespace ImGui = ImGuiMCP;

namespace OARConverterUI {
    constexpr const char* MOD_DIR = "Data/Viny Mods/DMK";
    constexpr const char* EXPORT_DIR = "Data/Viny Mods/DMK/Export";
    constexpr const char* LEGACY_DIR = "Data/Viny Mods/DMK/Legacy";
    constexpr const char* SETTINGS_PATH = "Data/Viny Mods/DMK/Settings.json";
    constexpr const char* LANG_PATH = "Data/Viny Mods/DMK/Language.json";
    constexpr const char* EXPORT_PATH = "Data/Viny Mods/DMK/Export/DirectionalConverted.zip";
    constexpr const char* OLD_SETTINGS_PATH = "Data/SKSE/Plugins/DMK_Settings.json";
    constexpr const char* OLD_LANG_PATH = "Data/SKSE/Plugins/DMK_Language.json";
    constexpr const char* OLD_EXPORT_PATH = "Data/export/DirectionalConverted.zip";
    static std::unordered_map<std::string, std::string> LangMap;

    fs::path GetAvailableLegacyPath(const fs::path& a_source)
    {
        fs::path destination = fs::path(LEGACY_DIR) / a_source.filename();
        for (int suffix = 1; fs::exists(destination); ++suffix) {
            destination = fs::path(LEGACY_DIR) /
                (a_source.stem().string() + "_" + std::to_string(suffix) + a_source.extension().string());
        }
        return destination;
    }

    void MoveLegacyFile(const fs::path& a_source, const fs::path& a_destination)
    {
        std::error_code ec;
        if (!fs::exists(a_source, ec) || ec) return;

        fs::path destination = a_destination;
        if (fs::exists(destination, ec) && !ec) destination = GetAvailableLegacyPath(a_source);

        fs::create_directories(destination.parent_path(), ec);
        if (ec) {
            SKSE::log::warn("[Settings] Falha ao criar pasta de migracao '{}': {}", destination.parent_path().string(), ec.message());
            return;
        }

        fs::rename(a_source, destination, ec);
        if (ec) {
            ec.clear();
            fs::copy_file(a_source, destination, fs::copy_options::none, ec);
            if (!ec) {
                std::error_code removeError;
                fs::remove(a_source, removeError);
                if (removeError) ec = removeError;
            }
        }

        if (ec) {
            SKSE::log::warn("[Settings] Falha ao migrar '{}' para '{}': {}", a_source.string(), destination.string(), ec.message());
        }
        else {
            SKSE::log::info("[Settings] Arquivo legado migrado de '{}' para '{}'.", a_source.string(), destination.string());
        }
    }

    void MigrateLegacyFiles()
    {
        static bool migrated = false;
        if (migrated) return;
        migrated = true;

        MoveLegacyFile(OLD_SETTINGS_PATH, SETTINGS_PATH);
        MoveLegacyFile(OLD_LANG_PATH, LANG_PATH);
        MoveLegacyFile(OLD_EXPORT_PATH, EXPORT_PATH);
    }

    void LoadLanguage() {
        MigrateLegacyFiles();
        LangMap.clear();
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            file.open(OLD_LANG_PATH, std::ios::binary);
            if (!file.is_open()) {
                SKSE::log::warn("Não foi possível carregar Language.json. Usando textos padrões.");
                return;
            }
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        if (jsonStr.size() >= 3 && (unsigned char)jsonStr[0] == 0xEF && (unsigned char)jsonStr[1] == 0xBB && (unsigned char)jsonStr[2] == 0xBF) {
            jsonStr.erase(0, 3);
        }

        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

        if (doc.HasParseError()) return;

        if (doc.IsObject()) {
            for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
                if (itr->value.IsObject()) {
                    std::string category = itr->name.GetString();
                    for (auto jtr = itr->value.MemberBegin(); jtr != itr->value.MemberEnd(); ++jtr) {
                        if (jtr->value.IsString()) {
                            LangMap[category + "." + jtr->name.GetString()] = jtr->value.GetString();
                        }
                    }
                }
                else if (itr->value.IsString()) {
                    LangMap[itr->name.GetString()] = itr->value.GetString();
                }
            }
        }
    }

    const char* GetLoc(const std::string& key, const char* defaultVal) {
        auto it = LangMap.find(key);
        if (it != LangMap.end()) return it->second.c_str();
        return defaultVal;
    }

    // 2. Helper para combinar direções em uma Diagonal (Padrão 8-Way)
    int CombineDirections(int dir1, int dir2) {
        // Diagonais padrão (8-Way)
        if ((dir1 == 1 && dir2 == 3) || (dir1 == 3 && dir2 == 1)) return 2; // Frente-Direita
        if ((dir1 == 5 && dir2 == 3) || (dir1 == 3 && dir2 == 5)) return 4; // Trás-Direita
        if ((dir1 == 5 && dir2 == 7) || (dir1 == 7 && dir2 == 5)) return 6; // Trás-Esquerda
        if ((dir1 == 1 && dir2 == 7) || (dir1 == 7 && dir2 == 1)) return 8; // Frente-Esquerda

        // Teclas Opostas (Novo)
        if ((dir1 == 1 && dir2 == 5) || (dir1 == 5 && dir2 == 1)) return 9; // Frente-Trás
        if ((dir1 == 7 && dir2 == 3) || (dir1 == 3 && dir2 == 7)) return 10; // Esquerda-Direita

        return dir1; // Retorno de segurança
    }

    // 3. Monta a nova estrutura do JSON substituindo a antiga
    void ReplaceWithNewCondition(rapidjson::Value& cond, const KeytraceInfo& info, bool isNegated, rapidjson::Document::AllocatorType& allocator) {
        cond.RemoveAllMembers();

        cond.AddMember("condition", "CompareValues", allocator);
        cond.AddMember("negated", isNegated, allocator);
        cond.AddMember("requiredVersion", "1.0.0.0", allocator);

        if (info.isDirectional) {
            rapidjson::Value valA(rapidjson::kObjectType);
            valA.AddMember("value", info.dirValue, allocator);
            cond.AddMember("Value A", valA, allocator);

            cond.AddMember("Comparison", "==", allocator);

            rapidjson::Value valB(rapidjson::kObjectType);
            valB.AddMember("graphVariable", "DirecionalCycleMoveset", allocator);
            valB.AddMember("graphVariableType", "Int", allocator);
            cond.AddMember("Value B", valB, allocator);
        }
        else {
            // Se for bool, preenche como "True" / "Bool"
            rapidjson::Value valA(rapidjson::kObjectType);
            valA.AddMember("value", 1, allocator);
            cond.AddMember("Value A", valA, allocator);

            cond.AddMember("Comparison", "==", allocator);

            rapidjson::Value valB(rapidjson::kObjectType);
            rapidjson::Value stringVarName;
            stringVarName.SetString(info.boolVarName.c_str(), allocator);
            valB.AddMember("graphVariable", stringVarName, allocator);
            valB.AddMember("graphVariableType", "Bool", allocator);
            cond.AddMember("Value B", valB, allocator);
        }
    }

    // 4. Busca recursivamente, avalia arrays e junta as diagonais se necessário
    void TraverseAndReplace(rapidjson::Value& node, rapidjson::Document::AllocatorType& allocator, int& modifiedCount, bool isAndContext = true) {

        // --- NOVIDADE: Navegação em Objetos (Nós Raiz, AND e OR) ---
        if (node.IsObject()) {
            bool currentContextIsAnd = isAndContext;

            // Verifica se este objeto altera o contexto lógico (ex: nó "OR" não pode somar combos)
            if (node.HasMember("condition") && node["condition"].IsString()) {
                std::string cType = node["condition"].GetString();
                if (cType == "OR" || cType == "XOR") {
                    currentContextIsAnd = false;
                }
                else if (cType == "AND") {
                    currentContextIsAnd = true;
                }
            }

            // Percorre todos os membros do Objeto procurando chaves que contenham Arrays
            for (auto itr = node.MemberBegin(); itr != node.MemberEnd(); ++itr) {
                // Se o membro for a lista de condições ("conditions" ou "Conditions")
                if (itr->name == "conditions" || itr->name == "Conditions") {
                    TraverseAndReplace(itr->value, allocator, modifiedCount, currentContextIsAnd);
                }
                else {
                    // Repassa a recursividade para as outras chaves do objeto
                    TraverseAndReplace(itr->value, allocator, modifiedCount, isAndContext);
                }
            }
        }

        // --- CÓDIGO DE ARRAYS (Onde a conversão realmente acontece) ---
        else if (node.IsArray()) {
            std::vector<rapidjson::SizeType> positiveDirIndices;
            std::vector<int> positiveDirValues;

            std::vector<rapidjson::SizeType> negatedDirIndices;
            std::vector<int> negatedDirValues;

            std::vector<rapidjson::SizeType> indicesToErase;

            // Etapa A: Separar Direcionais Positivos e Negados
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                KeytraceInfo info = ParseKeytraceCondition(node[i]);
                if (info.isValid && info.isDirectional) {
                    bool isNegated = false;
                    if (node[i].HasMember("negated") && node[i]["negated"].IsBool()) {
                        isNegated = node[i]["negated"].GetBool();
                    }

                    if (isNegated) {
                        negatedDirIndices.push_back(i);
                        negatedDirValues.push_back(info.dirValue);
                    }
                    else {
                        positiveDirIndices.push_back(i);
                        positiveDirValues.push_back(info.dirValue);
                    }
                }
            }

            // Etapa B: Filtrar Duplicatas e Agrupar APENAS os Positivos
            std::vector<int> uniqueDirValues;
            std::vector<rapidjson::SizeType> uniqueDirIndices;

            // Limpa modders que colocaram a mesma direção duas vezes no arquivo
            for (size_t i = 0; i < positiveDirIndices.size(); i++) {
                int val = positiveDirValues[i];
                if (std::find(uniqueDirValues.begin(), uniqueDirValues.end(), val) == uniqueDirValues.end()) {
                    uniqueDirValues.push_back(val);
                    uniqueDirIndices.push_back(positiveDirIndices[i]);
                }
                else {
                    // É uma duplicata inútil, já marcamos para deletar do JSON
                    indicesToErase.push_back(positiveDirIndices[i]);
                }
            }

            bool grouped = false; // Flag de agrupamento

            if (isAndContext && uniqueDirValues.size() >= 2) {
                if (uniqueDirValues.size() == 3) {
                    int sum = uniqueDirValues[0] + uniqueDirValues[1] + uniqueDirValues[2];
                    int combinedDir = 0;

                    if (sum == 11) combinedDir = 11;      // Frente(1) + Esquerda(7) + Direita(3)
                    else if (sum == 9) combinedDir = 12;  // Frente(1) + Trás(5) + Direita(3)
                    else if (sum == 15) combinedDir = 13; // Trás(5) + Esquerda(7) + Direita(3)
                    else if (sum == 13) combinedDir = 14; // Frente(1) + Trás(5) + Esquerda(7)

                    if (combinedDir != 0) {
                        KeytraceInfo comboInfo;
                        comboInfo.isDirectional = true;
                        comboInfo.dirValue = combinedDir;

                        ReplaceWithNewCondition(node[uniqueDirIndices[0]], comboInfo, false, allocator);
                        modifiedCount++;

                        indicesToErase.push_back(uniqueDirIndices[1]);
                        indicesToErase.push_back(uniqueDirIndices[2]);
                        grouped = true;
                    }
                }
                else if (uniqueDirValues.size() == 2) {
                    int combinedDir = CombineDirections(uniqueDirValues[0], uniqueDirValues[1]);

                    KeytraceInfo comboInfo;
                    comboInfo.isDirectional = true;
                    comboInfo.dirValue = combinedDir;

                    ReplaceWithNewCondition(node[uniqueDirIndices[0]], comboInfo, false, allocator);
                    modifiedCount++;

                    indicesToErase.push_back(uniqueDirIndices[1]);
                    grouped = true;
                }
            }

            // Fallback: Se não agrupou, ou é só uma tecla, converte individualmente
            if (!grouped && !uniqueDirIndices.empty()) {
                for (size_t i = 0; i < uniqueDirIndices.size(); i++) {
                    KeytraceInfo dInfo;
                    dInfo.isDirectional = true;
                    dInfo.dirValue = uniqueDirValues[i];
                    ReplaceWithNewCondition(node[uniqueDirIndices[i]], dInfo, false, allocator);
                    modifiedCount++;
                }
            }

            // Etapa C: Processar os NEGADOS sempre individualmente
            for (size_t i = 0; i < negatedDirIndices.size(); i++) {
                KeytraceInfo dInfo;
                dInfo.isDirectional = true;
                dInfo.dirValue = negatedDirValues[i];
                ReplaceWithNewCondition(node[negatedDirIndices[i]], dInfo, true, allocator);
                modifiedCount++;
            }

            // Etapa D: Processar as Chaves Adicionais (Esquiva/Ataque etc)
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                if (std::find(indicesToErase.begin(), indicesToErase.end(), i) != indicesToErase.end()) continue;

                KeytraceInfo info = ParseKeytraceCondition(node[i]);
                if (info.isValid && !info.isDirectional) {
                    bool isNegated = false;
                    if (node[i].HasMember("negated") && node[i]["negated"].IsBool()) {
                        isNegated = node[i]["negated"].GetBool();
                    }
                    ReplaceWithNewCondition(node[i], info, isNegated, allocator);
                    modifiedCount++;
                }
            }

            // Etapa E: Continuar recursividade nos filhos restantes
            for (rapidjson::SizeType i = 0; i < node.Size(); i++) {
                if (std::find(indicesToErase.begin(), indicesToErase.end(), i) != indicesToErase.end()) continue;
                TraverseAndReplace(node[i], allocator, modifiedCount, isAndContext);
            }

            // Etapa F: Apagar do JSON as redundâncias e duplicatas
            std::sort(indicesToErase.rbegin(), indicesToErase.rend());
            for (rapidjson::SizeType idx : indicesToErase) {
                node.Erase(node.Begin() + idx);
            }
        }
    }

    // --- HELPER: Converte o caminho para UTF-8 de forma segura para usar no log do SKSE ---
    std::string PathToUtf8String(const fs::path& p) {
        try {
            auto u8 = p.generic_u8string();
            return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
        }
        catch (...) {
            return "Caminho com caracteres ilegiveis";
        }
    }

    std::string PathToLogString(const fs::path& p) {
        return PathToUtf8String(p);
    }

    // 5. Processamento do arquivo OAR (Agora armazena na memória antes de escrever no disco)
    void ProcessOARJson(const fs::path& filepath, int& totalFilesModified, int& totalConditionsModified, int& totalScanned, std::vector<ConvertedFile>& modifiedFilesList, bool exportToZip) {
        std::string logPath = PathToLogString(filepath);
        totalScanned++; // Incrementa contador de arquivos lidos

        try {
            std::ifstream ifs(filepath);
            if (!ifs.is_open()) return;

            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            ifs.close();

            if (doc.HasParseError() || !doc.IsObject()) return;

            int modifiedInThisFile = 0;
            // A chamada inicial começa o arquivo considerando que está tudo num contexto "AND"
            TraverseAndReplace(doc, doc.GetAllocator(), modifiedInThisFile, true);

            if (modifiedInThisFile > 0) {

                // Grava o JSON modificado em uma string na memória
                rapidjson::StringBuffer buffer;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                std::string modifiedJsonString = buffer.GetString();

                // SE NÃO FOR EXPORTAR PARA ZIP, SOBRESCREVE O ARQUIVO ORIGINAL
                if (!exportToZip) {
                    std::ofstream ofs(filepath);
                    if (ofs.is_open()) {
                        ofs << modifiedJsonString;
                        ofs.close();
                    }
                }

                totalFilesModified++;
                totalConditionsModified += modifiedInThisFile;

                // Adiciona na nossa lista de arquivos (com o conteúdo) para a criação do ZIP
                modifiedFilesList.push_back({ filepath, modifiedJsonString });

                SKSE::log::info("Sucesso! Processado: {} ({} condicoes alteradas)", logPath, modifiedInThisFile);
            }
        }
        catch (const std::exception& e) {
            SKSE::log::error("Erro no arquivo [{}]: {}", logPath, e.what());
        }
    }

    // 6. Varredura de pastas Atualizada (Recebe a flag 'exportToZip')
    void RunConversionProcess(std::vector<ConvertedFile>& modifiedFilesList, bool exportToZip) {
        SKSE::log::info("Iniciando escaneamento para conversao de condicoes OAR...");
        try {
            fs::path startPath = "Data/meshes";
            if (!fs::exists(startPath)) {
                SKSE::log::warn("A pasta Data/meshes nao foi encontrada.");
                return;
            }

            int totalFiles = 0;
            int totalConditions = 0;
            int totalScanned = 0;

            for (const auto& entry : fs::recursive_directory_iterator(startPath)) {
                if (entry.is_regular_file()) {

                    // Pega o nome do arquivo em minúsculas
                    std::wstring filename = ToLowerW(entry.path().filename().wstring());

                    // Modificação: Removida a trava da pasta "openanimationreplacer"
                    // Agora varre qualquer config.json ou user.json encontrado
                    if (filename == L"config.json" || filename == L"user.json") {
                        ProcessOARJson(entry.path(), totalFiles, totalConditions, totalScanned, modifiedFilesList, exportToZip);
                    }
                }
            }

            SKSE::log::info("=== CONVERSAO OAR CONCLUIDA ===");
            SKSE::log::info("Arquivos config/user.json lidos: {}", totalScanned);
            SKSE::log::info("Arquivos processados: {}", totalFiles);
            SKSE::log::info("Condicoes substituidas: {}", totalConditions);
        }
        catch (const std::filesystem::filesystem_error& e) {
            SKSE::log::critical("Erro de sistema de arquivos: {}", e.what());
        }
        catch (const std::exception& e) {
            SKSE::log::critical("Erro fatal durante a varredura: {}", e.what());
        }
    }

    // 7. Salva os arquivos modificados em um ZIP (Escrevendo direto da memória para o ZIP)
    void ExportConvertedFilesToZip(const std::vector<ConvertedFile>& convertedFiles) {
        if (convertedFiles.empty()) {
            return;
        }

        try {
            fs::path exportDir = EXPORT_DIR;
            fs::create_directories(exportDir); // Garante que a pasta existe

            std::string zipPath = PathToUtf8String(EXPORT_PATH);

            mz_zip_archive zip_archive;
            memset(&zip_archive, 0, sizeof(zip_archive));

            if (!mz_zip_writer_init_file(&zip_archive, zipPath.c_str(), 0)) {
                SKSE::log::error("Export OAR: Falha ao inicializar arquivo ZIP em {}", zipPath);
                return;
            }

            for (const auto& file : convertedFiles) {
                std::string sourcePath = PathToUtf8String(file.originalPath);

                // O arquivo já deve ter um path como "Data\meshes\...", ajustamos apenas as barras
                std::string internalZipPath = sourcePath;
                std::replace(internalZipPath.begin(), internalZipPath.end(), '\\', '/');

                // USANDO mz_zip_writer_add_mem PARA LER A STRING DA MEMÓRIA DIRETO PARA O ZIP
                if (!mz_zip_writer_add_mem(&zip_archive, internalZipPath.c_str(), file.modifiedContent.data(), file.modifiedContent.size(), MZ_BEST_COMPRESSION)) {
                    SKSE::log::error("Export OAR: Falha ao adicionar arquivo {} ao ZIP", internalZipPath);
                }
                else {
                    SKSE::log::info("Export OAR: Adicionado ao ZIP: {}", internalZipPath);
                }
            }

            mz_zip_writer_finalize_archive(&zip_archive);
            mz_zip_writer_end(&zip_archive);

            SKSE::log::info("Exportacao OAR concluida com sucesso para: {}", zipPath);
        }
        catch (const std::exception& e) {
            SKSE::log::error("Export OAR: excecao durante exportacao ZIP: {}", e.what());
        }
        catch (...) {
            SKSE::log::error("Export OAR: excecao desconhecida durante exportacao ZIP.");
        }
    }

    void WriteActionIDs(rapidjson::Value& a_doc, rapidjson::Document::AllocatorType& a_alloc, const char* a_name, const std::vector<int>& a_ids)
    {
        rapidjson::Value values(rapidjson::kArrayType);
        for (int id : a_ids) values.PushBack(id, a_alloc);
        rapidjson::Value name;
        name.SetString(a_name, a_alloc);
        a_doc.AddMember(name, values, a_alloc);
    }

    void ReadActionIDs(const rapidjson::Value& a_doc, const char* a_name, std::vector<int>& a_ids)
    {
        if (!a_doc.HasMember(a_name) || !a_doc[a_name].IsArray()) return;
        a_ids.clear();
        for (const auto& value : a_doc[a_name].GetArray()) {
            if (value.IsInt()) a_ids.push_back(value.GetInt());
        }
    }

    void UnregisterActionList(const std::vector<int>& a_ids, const char* a_purpose)
    {
        if (!InputManagerAPI::_API) return;
        for (int id : a_ids) {
            InputManagerAPI::_API->UpdateListener(0, id, "Directional Movement Keys", a_purpose, false, nullptr, 0, nullptr, 0);
        }
    }

    void RegisterAllInputs()
    {
        if (!InputManagerAPI::_API || !UseInputManagerExtendedKeys) return;
        for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
            for (int id : ExtendedKeyActionIDs[i]) {
                InputManagerAPI::_API->UpdateListener(0, id, "Directional Movement Keys", ExtendedKeyLabels[i], true, nullptr, 0, nullptr, 0);
            }
        }
    }

    void UnregisterAllInputs()
    {
        if (!InputManagerAPI::_API) return;
        for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
            UnregisterActionList(ExtendedKeyActionIDs[i], ExtendedKeyLabels[i]);
        }
    }

    void TweenPauseRegister()
    {
        if (!UseInputManagerExtendedKeys) return;
        auto* dispatcher = SKSE::GetModCallbackEventSource();
        if (!dispatcher) return;

        for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
            rapidjson::Document doc;
            doc.SetObject();
            auto& alloc = doc.GetAllocator();
            doc.AddMember("tabId", "gameplay", alloc);
            doc.AddMember("tabLabel", "Mods", alloc);
            doc.AddMember("categoryId", "directionalMovementKeys", alloc);
            doc.AddMember("categoryLabel", "Directional Movement Keys", alloc);

            const std::string actionId = std::string("DMKExtended_") + ExtendedKeyIds[i];
            rapidjson::Value actionIdValue;
            actionIdValue.SetString(actionId.c_str(), alloc);
            doc.AddMember("actionId", actionIdValue, alloc);
            rapidjson::Value actionLabel;
            actionLabel.SetString(ExtendedKeyLabels[i], alloc);
            doc.AddMember("actionLabel", actionLabel, alloc);
            doc.AddMember("acceptsMotion", false, alloc);

            rapidjson::Value mappedIds(rapidjson::kArrayType);
            for (int id : ExtendedKeyActionIDs[i]) {
                rapidjson::Value bind(rapidjson::kObjectType);
                bind.AddMember("id", id, alloc);
                bind.AddMember("type", "action", alloc);
                mappedIds.PushBack(bind, alloc);
            }
            doc.AddMember("mappedIds", mappedIds, alloc);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            SKSE::ModCallbackEvent event{ "TweenPause_RegisterControl", RE::BSFixedString(buffer.GetString()), 0.0f, nullptr };
            dispatcher->SendEvent(&event);
        }
    }

    bool HandleTweenPauseControlUpdate(const char* a_payload)
    {
        if (!a_payload || !UseInputManagerExtendedKeys) return false;
        rapidjson::Document doc;
        doc.Parse(a_payload);
        if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("actionId") || !doc["actionId"].IsString()) return false;

        const std::string_view actionId = doc["actionId"].GetString();
        for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
            const std::string expectedId = std::string("DMKExtended_") + ExtendedKeyIds[i];
            if (actionId != expectedId) continue;

            const auto oldActions = ExtendedKeyActionIDs[i];
            std::vector<int> newActions;
            if (doc.HasMember("mappedIds") && doc["mappedIds"].IsArray()) {
                for (const auto& bind : doc["mappedIds"].GetArray()) {
                    if (bind.IsObject() && bind.HasMember("actionID") && bind["actionID"].IsInt()) {
                        newActions.push_back(bind["actionID"].GetInt());
                    }
                }
            }

            UnregisterActionList(oldActions, ExtendedKeyLabels[i]);
            ExtendedKeyActionIDs[i] = std::move(newActions);
            RegisterAllInputs();
            SaveSettings();
            return true;
        }
        return false;
    }

    void SaveSettings() {
        std::error_code directoryError;
        fs::create_directories(MOD_DIR, directoryError);
        if (directoryError) {
            SKSE::log::error("[Settings] Falha ao criar '{}': {}", MOD_DIR, directoryError.message());
            return;
        }

        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        doc.AddMember("NPCOnlyCombat", NPCOnlyCombat, alloc);
        doc.AddMember("NPCAttackDirectionAtWeaponSwing", NPCAttackDirectionAtWeaponSwing, alloc);
        doc.AddMember("NPCAttackDirectionFallback", std::clamp(NPCAttackDirectionFallback, 0, 8), alloc);
        doc.AddMember("DirectionalMode", DirectionalMode, alloc);
        doc.AddMember("UseInputManagerExtendedKeys", UseInputManagerExtendedKeys, alloc);
        doc.AddMember("CameraSensitivity", CameraSensitivity, alloc);
        doc.AddMember("CameraMinimumDistance", CameraMinimumDistance, alloc);
        doc.AddMember("EnableCameraAutoReset", EnableCameraAutoReset, alloc);
        doc.AddMember("CameraResetDelaySeconds", CameraResetDelaySeconds, alloc);
        for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
            const std::string name = std::string(ExtendedKeyIds[i]) + "ActionIDs";
            WriteActionIDs(doc, alloc, name.c_str(), ExtendedKeyActionIDs[i]);
        }

        FILE* fp = nullptr;
        fopen_s(&fp, SETTINGS_PATH, "wb");
        if (fp) {
            char writeBuffer[65536];
            rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
            rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
            doc.Accept(writer);
            fclose(fp);
        }
    }

    void LoadSettings() {
        MigrateLegacyFiles();
        FILE* fp = nullptr;
        fopen_s(&fp, SETTINGS_PATH, "rb");
        if (!fp) fopen_s(&fp, OLD_SETTINGS_PATH, "rb");
        if (fp) {
            char readBuffer[65536];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            rapidjson::Document doc;
            doc.ParseStream(is);
            fclose(fp);

            if (doc.IsObject()) {
                if (doc.HasMember("NPCOnlyCombat")) NPCOnlyCombat = doc["NPCOnlyCombat"].GetBool();
                if (doc.HasMember("NPCAttackDirectionAtWeaponSwing") && doc["NPCAttackDirectionAtWeaponSwing"].IsBool()) {
                    NPCAttackDirectionAtWeaponSwing = doc["NPCAttackDirectionAtWeaponSwing"].GetBool();
                }
                else if (doc.HasMember("NPCWarningUseWeaponSwing") && doc["NPCWarningUseWeaponSwing"].IsBool()) {
                    NPCAttackDirectionAtWeaponSwing = doc["NPCWarningUseWeaponSwing"].GetBool();
                }
                if (doc.HasMember("NPCAttackDirectionFallback") && doc["NPCAttackDirectionFallback"].IsInt()) {
                    NPCAttackDirectionFallback = std::clamp(doc["NPCAttackDirectionFallback"].GetInt(), 0, 8);
                }
                if (doc.HasMember("DirectionalMode")) DirectionalMode = doc["DirectionalMode"].GetBool();
                if (doc.HasMember("UseInputManagerExtendedKeys") && doc["UseInputManagerExtendedKeys"].IsBool()) UseInputManagerExtendedKeys = doc["UseInputManagerExtendedKeys"].GetBool();
                if (doc.HasMember("CameraSensitivity") && doc["CameraSensitivity"].IsNumber()) CameraSensitivity = std::clamp(doc["CameraSensitivity"].GetFloat(), 0.05f, 2.0f);
                if (doc.HasMember("CameraMinimumDistance") && doc["CameraMinimumDistance"].IsNumber()) CameraMinimumDistance = std::clamp(doc["CameraMinimumDistance"].GetFloat(), 1.0f, 299.0f);
                if (doc.HasMember("EnableCameraAutoReset") && doc["EnableCameraAutoReset"].IsBool()) EnableCameraAutoReset = doc["EnableCameraAutoReset"].GetBool();
                if (doc.HasMember("CameraResetDelaySeconds") && doc["CameraResetDelaySeconds"].IsNumber()) {
                    CameraResetDelaySeconds = std::clamp(doc["CameraResetDelaySeconds"].GetFloat(), 0.05f, 10.0f);
                }
                else if (doc.HasMember("CameraResetDelayMs") && doc["CameraResetDelayMs"].IsNumber()) {
                    CameraResetDelaySeconds = std::clamp(doc["CameraResetDelayMs"].GetFloat() / 1000.0f, 0.05f, 10.0f);
                }
                for (std::size_t i = 0; i < ExtendedKeyCount; ++i) {
                    const std::string name = std::string(ExtendedKeyIds[i]) + "ActionIDs";
                    ReadActionIDs(doc, name.c_str(), ExtendedKeyActionIDs[i]);
                }
            }
        }
    }

    void RenderMenu()
    {
        static bool showConfirmPopup = false;
        static bool showSuccessMessage = false;
        static bool exportToZip = true;

        ImGuiMCP::Text("%s", GetLoc("menu.oar_title", "OAR Conditions Converter"));
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        ImGuiMCP::TextWrapped("%s", GetLoc("menu.oar_desc1", "It finds Dtry conditions (Keytrace.esp directional magic effects)"));
        ImGuiMCP::TextWrapped("%s", GetLoc("menu.oar_desc2", "and replaces them with the new DirecionalCycleMoveset Graph Variable."));
        ImGuiMCP::Spacing();

        ImGuiMCP::Checkbox(GetLoc("menu.oar_export_zip", "Export converted files to ZIP (Do NOT touch original files)"), &exportToZip);
        ImGuiMCP::Spacing();

        if (ImGuiMCP::Button(GetLoc("menu.oar_convert_btn", "Convert Dtry conditions"), { 250, 40 })) {
            showConfirmPopup = true;
            showSuccessMessage = false;
        }

        if (showSuccessMessage) {
            ImGuiMCP::Spacing();
            ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.oar_success", "Conversion completed successfully! Check SKSE logs for details."));
            if (exportToZip) {
                ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.oar_success_zip", "Zip created at Data/Viny Mods/Directional Movement Keys/Export/DirectionalConverted.zip (Original files untouched)"));
            }
        }

        if (showConfirmPopup) {
            ImGuiMCP::OpenPopup(GetLoc("menu.oar_confirm_popup", "Confirm Conversion"));
        }

        if (ImGuiMCP::BeginPopupModal(GetLoc("menu.oar_confirm_popup", "Confirm Conversion"), nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {

            if (exportToZip) {
                ImGuiMCP::Text("%s", GetLoc("menu.oar_confirm_zip1", "Are you sure you want to extract the conditions?"));
                ImGuiMCP::Text("%s", GetLoc("menu.oar_confirm_zip2", "Original files will NOT be modified. A ZIP file will be created."));
            }
            else {
                ImGuiMCP::Text("%s", GetLoc("menu.oar_confirm_replace1", "Are you sure you want to convert the conditions?"));
                ImGuiMCP::Text("%s", GetLoc("menu.oar_confirm_replace2", "This change is permanent. Your original files WILL BE MODIFIED."));
            }

            ImGuiMCP::Text("%s", GetLoc("menu.oar_confirm_restart", "You will need to restart the game after finishing."));
            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            if (ImGuiMCP::Button(GetLoc("menu.yes_convert", "Yes, Convert"), { 120, 0 })) {

                std::vector<ConvertedFile> modifiedFiles;
                RunConversionProcess(modifiedFiles, exportToZip);

                if (exportToZip && !modifiedFiles.empty()) {
                    ExportConvertedFilesToZip(modifiedFiles);
                }

                showConfirmPopup = false;
                showSuccessMessage = true;
                ImGuiMCP::CloseCurrentPopup();
            }

            ImGuiMCP::SameLine();

            if (ImGuiMCP::Button(GetLoc("common.cancel", "Cancel"), { 120, 0 })) {
                showConfirmPopup = false;
                ImGuiMCP::CloseCurrentPopup();
            }
            ImGuiMCP::EndPopup();
        }
    }

    std::string ToLowerString(std::string a_value)
    {
        std::transform(a_value.begin(), a_value.end(), a_value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return a_value;
    }

    int GetIndexFromID(int a_id, const int* a_ids, int a_count)
    {
        for (int i = 0; i < a_count; ++i) {
            if (a_ids[i] == a_id) return i;
        }
        return 0;
    }

    bool SearchableCombo(const char* a_label, int* a_currentItem, const char* const a_items[], int a_count)
    {
        bool changed = false;
        const char* preview = *a_currentItem >= 0 && *a_currentItem < a_count ? a_items[*a_currentItem] : GetLoc("common.none", "None");
        if (ImGui::BeginCombo(a_label, preview)) {
            static char search[128]{};
            if (ImGui::IsWindowAppearing()) {
                search[0] = '\0';
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText(GetLoc("common.search_placeholder", "Filter..."), search, sizeof(search));
            ImGui::Separator();

            const std::string filter = ToLowerString(search);
            for (int i = 0; i < a_count; ++i) {
                if (!filter.empty() && ToLowerString(a_items[i]).find(filter) == std::string::npos) continue;
                const bool selected = *a_currentItem == i;
                if (ImGui::Selectable(a_items[i], selected)) {
                    *a_currentItem = i;
                    changed = true;
                }
                if (selected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    const char* GetActionStateName(int a_action)
    {
        return a_action >= 0 && a_action < static_cast<int>(std::size(actionStateNames)) ? actionStateNames[a_action] : actionStateNames[0];
    }

    std::string GetDirectionalInputName(std::uint32_t a_key)
    {
        switch (a_key) {
        case InputManagerAPI::VKEY_DIR_UP: return "Up";
        case InputManagerAPI::VKEY_DIR_DOWN: return "Down";
        case InputManagerAPI::VKEY_DIR_LEFT: return "Left";
        case InputManagerAPI::VKEY_DIR_RIGHT: return "Right";
        case InputManagerAPI::VKEY_DIR_UPRIGHT: return "Up-Right";
        case InputManagerAPI::VKEY_DIR_UPLEFT: return "Up-Left";
        case InputManagerAPI::VKEY_DIR_DOWNRIGHT: return "Down-Right";
        case InputManagerAPI::VKEY_DIR_DOWNLEFT: return "Down-Left";
        default: return {};
        }
    }

    std::string GetPCInputName(std::uint32_t a_key)
    {
        if (auto directional = GetDirectionalInputName(a_key); !directional.empty()) return directional;

        constexpr std::uint32_t mouseOffset = 256;
        constexpr const char* mouseNames[] = {
            "Mouse Left", "Mouse Right", "Mouse Middle", "Mouse 4", "Mouse 5",
            "Mouse 6", "Mouse 7", "Mouse 8", "Mouse Wheel Up", "Mouse Wheel Down"
        };
        if (a_key >= mouseOffset && a_key < mouseOffset + std::size(mouseNames)) {
            return mouseNames[a_key - mouseOffset];
        }

        char name[128]{};
        const LONG keyNameParam = static_cast<LONG>(a_key << 16);
        if (::GetKeyNameTextA(keyNameParam, name, static_cast<int>(std::size(name))) > 0) return name;
        return "Key " + std::to_string(a_key);
    }

    std::string GetGamepadInputName(std::uint32_t a_key)
    {
        if (auto directional = GetDirectionalInputName(a_key); !directional.empty()) return directional;

        for (std::size_t i = 0; i < std::size(gamepadKeyIDs); ++i) {
            if (static_cast<std::uint32_t>(gamepadKeyIDs[i]) == a_key) return gamepadKeyNames[i];
        }
        return "Gamepad " + std::to_string(a_key);
    }

    void DrawActionTooltip(int a_actionID)
    {
        const auto info = InputManagerAPI::_API->GetActionInfo(a_actionID);
        if (!info.isValid) {
            ImGui::TextDisabled("%s", GetLoc("menu.no_info", "No information available."));
            return;
        }

        const char* actionNames[] = { "Ignore", "Tap", "Hold", "Gesture", "Press" };
        auto actionName = [&actionNames](int a_action) {
            return a_action >= 0 && a_action < 5 ? actionNames[a_action] : "Unknown";
        };
        auto actionDescription = [&actionName](int a_action, int a_taps) {
            std::string result = actionName(a_action);
            if (a_action == 1) result += " x" + std::to_string(a_taps);
            return result;
        };

        ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.input_details", "Input Details"));
        ImGui::Separator();
        ImGui::Text("%s: %d | %s: %s", GetLoc("common.id", "ID"), info.id, GetLoc("common.name", "Name"), info.name ? info.name : GetLoc("common.unnamed", "Unnamed"));
        ImGui::Text("%s: %s (%s)", GetLoc("menu.pc_main_key", "PC Main Key"), GetPCInputName(info.pcMainKey).c_str(), actionDescription(info.pcMainAction, info.pcMainTapCount).c_str());
        if (info.pcModifierKey != 0) {
            const char* gestureName = info.pcModAction == 3 ? InputManagerAPI::_API->GetInputName(2, info.pcModifierKey) : nullptr;
            const std::string modifier = info.pcModAction == 3 ? (gestureName ? gestureName : "Unknown Gesture") : GetPCInputName(info.pcModifierKey);
            ImGui::Text("%s: %s (%s)", GetLoc("menu.pc_mod_key", "PC Modifier"), modifier.c_str(), actionDescription(info.pcModAction, info.pcModTapCount).c_str());
        }
        ImGui::Text("%s: %s (%s)", GetLoc("menu.pad_main_key", "Gamepad Main Key"), GetGamepadInputName(info.gamepadMainKey).c_str(), actionDescription(info.gamepadMainAction, info.gamepadMainTapCount).c_str());
        if (info.gamepadModifierKey != 0) {
            const char* gestureName = info.gamepadModAction == 3 ? InputManagerAPI::_API->GetInputName(2, info.gamepadModifierKey) : nullptr;
            const std::string modifier = info.gamepadModAction == 3 ? (gestureName ? gestureName : "Unknown Gesture") : GetGamepadInputName(info.gamepadModifierKey);
            ImGui::Text("%s: %s (%s)", GetLoc("menu.pad_mod_key", "Gamepad Modifier"), modifier.c_str(), actionDescription(info.gamepadModAction, info.gamepadModTapCount).c_str());
        }
        if (info.useCustomTimings) {
            ImGui::Text("%s: %.2fs | %s: %.2fs", GetLoc("menu.tap_window", "Tap Window"), info.tapWindow, GetLoc("menu.hold", "Hold"), info.holdDuration);
        }
    }

    void EditExtendedKeyActions(std::size_t a_slot)
    {
        auto& actions = ExtendedKeyActionIDs[a_slot];
        const auto oldActions = actions;
        bool changed = false;
        static int editingActionID = -1;
        static InputManagerAPI::ActionInfo editStaging{};
        static bool showEditError = false;
        bool openEditPopup = false;
        const std::string editPopupId = std::string("EditAction_") + ExtendedKeyIds[a_slot];

        ImGui::PushID(ExtendedKeyIds[a_slot]);
        if (ImGui::CollapsingHeader(ExtendedKeyLabels[a_slot])) {
            ImGui::Indent();
            for (std::size_t i = 0; i < actions.size();) {
                const int id = actions[i];
                const char* name = InputManagerAPI::_API->GetInputName(0, id);
                ImGui::Text("[Action] [%d] %s", id, name ? name : GetLoc("common.unnamed", "Unnamed"));
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    DrawActionTooltip(id);
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Button(GetLoc("common.edit", "Edit"))) {
                    editingActionID = id;
                    editStaging = InputManagerAPI::_API->GetActionInfo(id);
                    showEditError = false;
                    openEditPopup = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    actions.erase(actions.begin() + static_cast<std::ptrdiff_t>(i));
                    changed = true;
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++i;
            }

            if (openEditPopup) ImGui::OpenPopup(editPopupId.c_str());
            if (ImGui::BeginPopup(editPopupId.c_str())) {
                if (editingActionID >= 0 && editStaging.isValid) {
                    ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s: %s",
                        GetLoc("menu.editing_action", "Editing Action"),
                        editStaging.name ? editStaging.name : GetLoc("common.unnamed", "Unnamed"));
                    ImGui::Separator();

                    auto drawMainActionCombo = [](const char* a_label, int& a_action) {
                        if (ImGui::BeginCombo(a_label, GetActionStateName(a_action))) {
                            for (int action = 0; action < static_cast<int>(std::size(actionStateNames)); ++action) {
                                if (action == 3) continue;
                                const bool selected = a_action == action;
                                if (ImGui::Selectable(actionStateNames[action], selected)) a_action = action;
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    };
                    auto drawModifierActionCombo = [](const char* a_label, int& a_action, int a_mainAction) {
                        if (ImGui::BeginCombo(a_label, GetActionStateName(a_action))) {
                            for (int action = 0; action < static_cast<int>(std::size(actionStateNames)); ++action) {
                                if (action == 3 && a_mainAction != 2 && a_mainAction != 4) continue;
                                const bool selected = a_action == action;
                                if (ImGui::Selectable(actionStateNames[action], selected)) a_action = action;
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    };
                    auto drawGestureCombo = [](const char* a_label, std::uint32_t& a_gesture) {
                        const int count = static_cast<int>(InputManagerAPI::_API->GetInputCount(2));
                        const int current = static_cast<int>(a_gesture);
                        const char* preview = current >= 0 && current < count ? InputManagerAPI::_API->GetInputName(2, current) : GetLoc("common.none", "None");
                        if (ImGui::BeginCombo(a_label, preview ? preview : GetLoc("common.none", "None"))) {
                            for (int gesture = 0; gesture < count; ++gesture) {
                                const char* name = InputManagerAPI::_API->GetInputName(2, gesture);
                                const bool selected = current == gesture;
                                if (ImGui::Selectable(name ? name : GetLoc("common.unnamed", "Unnamed"), selected)) a_gesture = static_cast<std::uint32_t>(gesture);
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    };
                    auto drawStickCombo = [](const char* a_label, int& a_stick) {
                        const char* sticks[] = { GetLoc("menu.left_stick", "Left Stick"), GetLoc("menu.right_stick", "Right Stick") };
                        const char* preview = a_stick >= 0 && a_stick < 2 ? sticks[a_stick] : sticks[0];
                        if (ImGui::BeginCombo(a_label, preview)) {
                            for (int stick = 0; stick < 2; ++stick) {
                                const bool selected = a_stick == stick;
                                if (ImGui::Selectable(sticks[stick], selected)) a_stick = stick;
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    };

                    const int pcKeyCount = static_cast<int>(std::size(pcKeyIDs));
                    const int gamepadKeyCount = static_cast<int>(std::size(gamepadKeyIDs));

                    ImGui::TextColored({ 0.7f, 0.7f, 1.0f, 1.0f }, "%s", GetLoc("menu.pc_settings_header", "--- PC Settings ---"));
                    int pcMainIndex = GetIndexFromID(editStaging.pcMainKey, pcKeyIDs, pcKeyCount);
                    if (SearchableCombo(GetLoc("menu.pc_main_key", "PC Main Key"), &pcMainIndex, pcKeyNames, pcKeyCount)) editStaging.pcMainKey = pcKeyIDs[pcMainIndex];
                    drawMainActionCombo(GetLoc("menu.pc_main_action", "PC Main Action"), editStaging.pcMainAction);
                    if (editStaging.pcMainAction == 1) {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputInt(GetLoc("menu.pc_main_taps", "PC Main Taps"), &editStaging.pcMainTapCount);
                    }

                    if (editStaging.pcModAction == 3 && editStaging.pcMainAction != 2 && editStaging.pcMainAction != 4) editStaging.pcModAction = 0;
                    drawModifierActionCombo(GetLoc("menu.pc_mod_action", "PC Modifier Action"), editStaging.pcModAction, editStaging.pcMainAction);
                    if (editStaging.pcModAction == 3) {
                        drawGestureCombo(GetLoc("menu.pc_gesture", "PC Gesture"), editStaging.pcModifierKey);
                    }
                    else {
                        int pcModifierIndex = GetIndexFromID(editStaging.pcModifierKey, pcKeyIDs, pcKeyCount);
                        if (SearchableCombo(GetLoc("menu.pc_mod_key", "PC Modifier Key"), &pcModifierIndex, pcKeyNames, pcKeyCount)) editStaging.pcModifierKey = pcKeyIDs[pcModifierIndex];
                        if (editStaging.pcModAction == 1) {
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::InputInt(GetLoc("menu.pc_mod_taps", "PC Modifier Taps"), &editStaging.pcModTapCount);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::TextColored({ 0.7f, 1.0f, 0.7f, 1.0f }, "%s", GetLoc("menu.pad_settings_header", "--- Gamepad Settings ---"));
                    int gamepadMainIndex = GetIndexFromID(editStaging.gamepadMainKey, gamepadKeyIDs, gamepadKeyCount);
                    if (SearchableCombo(GetLoc("menu.pad_main_key", "Gamepad Main Key"), &gamepadMainIndex, gamepadKeyNames, gamepadKeyCount)) editStaging.gamepadMainKey = gamepadKeyIDs[gamepadMainIndex];
                    drawMainActionCombo(GetLoc("menu.pad_main_action", "Gamepad Main Action"), editStaging.gamepadMainAction);
                    if (editStaging.gamepadMainAction == 1) {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputInt(GetLoc("menu.pad_main_taps", "Gamepad Main Taps"), &editStaging.gamepadMainTapCount);
                    }

                    if (editStaging.gamepadModAction == 3 && editStaging.gamepadMainAction != 2 && editStaging.gamepadMainAction != 4) editStaging.gamepadModAction = 0;
                    drawModifierActionCombo(GetLoc("menu.pad_mod_action", "Gamepad Modifier Action"), editStaging.gamepadModAction, editStaging.gamepadMainAction);
                    if (editStaging.gamepadModAction == 3) {
                        drawGestureCombo(GetLoc("menu.pad_gesture", "Gamepad Gesture"), editStaging.gamepadModifierKey);
                        drawStickCombo(GetLoc("menu.pad_gesture_stick", "Gesture Stick"), editStaging.gamepadGestureStick);
                    }
                    else {
                        int gamepadModifierIndex = GetIndexFromID(editStaging.gamepadModifierKey, gamepadKeyIDs, gamepadKeyCount);
                        if (SearchableCombo(GetLoc("menu.pad_mod_key", "Gamepad Modifier Key"), &gamepadModifierIndex, gamepadKeyNames, gamepadKeyCount)) editStaging.gamepadModifierKey = gamepadKeyIDs[gamepadModifierIndex];
                        if (editStaging.gamepadModAction == 1) {
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::InputInt(GetLoc("menu.pad_mod_taps", "Gamepad Modifier Taps"), &editStaging.gamepadModTapCount);
                        }
                    }

                    ImGui::Separator();
                    if (showEditError) {
                        ImGui::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("menu.save_error", "Error: Conflict detected or invalid input!"));
                    }
                    if (ImGui::Button(GetLoc("common.save", "Save"), { 120.0f, 0.0f })) {
                        if (InputManagerAPI::_API->UpdateActionMapping(editingActionID, editStaging)) {
                            showEditError = false;
                            editingActionID = -1;
                            TweenPauseRegister();
                            ImGui::CloseCurrentPopup();
                        }
                        else {
                            showEditError = true;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(GetLoc("common.cancel", "Cancel"), { 120.0f, 0.0f })) {
                        editingActionID = -1;
                        showEditError = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                else {
                    ImGui::TextDisabled("%s", GetLoc("menu.no_info", "No information available."));
                }
                ImGui::EndPopup();
            }

            const std::string popupId = std::string("AddAction_") + ExtendedKeyIds[a_slot];
            if (ImGui::Button(GetLoc("menu.add_input", "+ Add Action"))) ImGui::OpenPopup(popupId.c_str());
            if (ImGui::BeginPopup(popupId.c_str())) {
                static char search[128] = "";
                if (ImGui::IsWindowAppearing()) search[0] = '\0';
                ImGui::InputText(GetLoc("common.search_placeholder", "Filter..."), search, sizeof(search));
                ImGui::Separator();
                ImGui::BeginChild("ActionList", { 360.0f, 220.0f }, true);

                std::string filter = search;
                std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                const auto count = InputManagerAPI::_API->GetInputCount(0);
                for (int id = 0; id < static_cast<int>(count); ++id) {
                    const char* name = InputManagerAPI::_API->GetInputName(0, id);
                    std::string label = "[" + std::to_string(id) + "] " + (name ? name : GetLoc("common.unnamed", "Unnamed"));
                    std::string searchable = label;
                    std::transform(searchable.begin(), searchable.end(), searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (!filter.empty() && searchable.find(filter) == std::string::npos) continue;

                    if (ImGui::Selectable(label.c_str(), false)) {
                        if (std::find(actions.begin(), actions.end(), id) == actions.end()) {
                            actions.push_back(id);
                            changed = true;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        DrawActionTooltip(id);
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
                ImGui::EndPopup();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();

        if (changed) {
            UnregisterActionList(oldActions, ExtendedKeyLabels[a_slot]);
            RegisterAllInputs();
            SaveSettings();
            TweenPauseRegister();
            Sink::InputListener::GetSingleton()->ResetManagedExtendedInputs();
            Sink::InputListener::GetSingleton()->ForceDirectionalUpdate();
        }
    }

    void MSettings()
    {
        bool changed = false;

        ImGuiMCP::Text("%s", GetLoc("menu.settings_title", "General Settings"));
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        // --- SEÇÃO DO PLAYER ---
        ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.player_section", "Player Configuration"));
        ImGuiMCP::Spacing();

        if (ImGuiMCP::Checkbox(GetLoc("menu.enable_custom_360", "Enable Custom 360° Directional Movement Logic"), &DirectionalMode)) {
            changed = true;
        }
        ImGuiMCP::TextWrapped("%s", GetLoc("menu.custom_360_desc", "Enables camera-relative calculations for 360 movement mods without requiring TDM API. Complex states (9 to 14) are disabled in this mode."));
        ImGuiMCP::Spacing();

        ImGuiMCP::SetNextItemWidth(280.0f);
        bool sensitivityChanged = ImGuiMCP::SliderFloat(GetLoc("menu.camera_sensitivity", "Camera Input Sensitivity"), &CameraSensitivity, 0.05f, 2.0f, "%.2f");
        bool sensitivityHovered = ImGuiMCP::IsItemHovered();
        ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(270.0f);
        sensitivityChanged |= ImGuiMCP::InputFloat("##CameraSensitivityInput", &CameraSensitivity, 0.01f, 0.10f, "%.2f");
        sensitivityHovered |= ImGuiMCP::IsItemHovered();
        if (sensitivityChanged) {
            CameraSensitivity = std::clamp(CameraSensitivity, 0.05f, 2.0f);
            changed = true;
        }
        if (sensitivityHovered) {
            ImGuiMCP::SetTooltip("%s", GetLoc("menu.camera_sensitivity_hover", "Multiplier applied to mouse camera movement before determining a direction."));
        }

        int cameraMinimumDistanceInput = static_cast<int>(std::lround(CameraMinimumDistance));
        ImGuiMCP::SetNextItemWidth(280.0f);
        bool distanceChanged = ImGuiMCP::SliderFloat(GetLoc("menu.camera_minimum_distance", "Camera Minimum Distance"), &CameraMinimumDistance, 1.0f, 299.0f, "%.0f");
        bool distanceHovered = ImGuiMCP::IsItemHovered();
        if (distanceChanged) cameraMinimumDistanceInput = static_cast<int>(std::lround(CameraMinimumDistance));
        ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(270.0f);
        if (ImGuiMCP::InputInt("##CameraMinimumDistanceInput", &cameraMinimumDistanceInput, 1, 10)) {
            CameraMinimumDistance = static_cast<float>(std::clamp(cameraMinimumDistanceInput, 1, 299));
            distanceChanged = true;
        }
        distanceHovered |= ImGuiMCP::IsItemHovered();
        if (distanceChanged) {
            changed = true;
        }
        if (distanceHovered) {
            ImGuiMCP::SetTooltip("%s", GetLoc("menu.camera_minimum_distance_hover", "Minimum accumulated mouse movement required before CameraMovementCMF changes direction."));
        }

        bool cameraResetChanged = ImGuiMCP::Checkbox(
            GetLoc("menu.camera_auto_reset", "Reset Camera Direction After Inactivity"),
            &EnableCameraAutoReset);
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip(
                "%s",
                GetLoc("menu.camera_auto_reset_hover", "Resets CameraMovementCMF to 0 when no camera input is received during the configured time."));
        }

        if (EnableCameraAutoReset) {
            float cameraResetDelayInput = CameraResetDelaySeconds;
            ImGuiMCP::SetNextItemWidth(280.0f);
            bool resetDelayChanged = ImGuiMCP::SliderFloat(
                GetLoc("menu.camera_reset_delay", "Camera Reset Delay (seconds)"),
                &CameraResetDelaySeconds,
                0.05f,
                10.0f,
                "%.2f");
            bool resetDelayHovered = ImGuiMCP::IsItemHovered();
            if (resetDelayChanged) cameraResetDelayInput = CameraResetDelaySeconds;
            ImGuiMCP::SameLine();
            ImGuiMCP::SetNextItemWidth(270.0f);
            if (ImGuiMCP::InputFloat("##CameraResetDelayInput", &cameraResetDelayInput, 0.05f, 0.5f, "%.2f")) {
                CameraResetDelaySeconds = std::clamp(cameraResetDelayInput, 0.05f, 10.0f);
                resetDelayChanged = true;
            }
            resetDelayHovered |= ImGuiMCP::IsItemHovered();
            if (resetDelayHovered) {
                ImGuiMCP::SetTooltip(
                    "%s",
                    GetLoc("menu.camera_reset_delay_hover", "Seconds without mouse or right-stick camera input before CameraMovementCMF returns to 0."));
            }
            cameraResetChanged |= resetDelayChanged;
        }

        if (cameraResetChanged) {
            Sink::InputListener::GetSingleton()->RefreshCameraResetTimer();
            changed = true;
        }
        ImGuiMCP::Spacing();

        if (InputManagerAPI::_API) {
            const bool wasEnabled = UseInputManagerExtendedKeys;
            if (ImGuiMCP::Checkbox(GetLoc("menu.input_manager_extended_keys", "Change Extended Keys with Input Manager"), &UseInputManagerExtendedKeys)) {
                if (wasEnabled) {
                    UnregisterAllInputs();
                }
                Sink::InputListener::GetSingleton()->ResetManagedExtendedInputs();
                if (UseInputManagerExtendedKeys) {
                    RegisterAllInputs();
                    TweenPauseRegister();
                }
                Sink::InputListener::GetSingleton()->ForceDirectionalUpdate();
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s", GetLoc("menu.input_manager_extended_keys_hover", "Replaces physical extended-key and fixed gamepad detection with Input Manager Actions."));
            }

            if (UseInputManagerExtendedKeys) {
                ImGuiMCP::Indent();
                for (std::size_t i = 0; i < ExtendedKeyCount; ++i) EditExtendedKeyActions(i);
                ImGuiMCP::Unindent();
            }
        }

        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        // --- SEÇÃO DOS NPCS ---
        ImGuiMCP::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.npc_section", "NPC Configuration"));
        ImGuiMCP::Spacing();

        if (ImGuiMCP::Checkbox(GetLoc("menu.npc_only_combat", "Evaluate NPC directional movement ONLY during combat"), &NPCOnlyCombat)) {
            changed = true;
        }
        if (ImGuiMCP::Checkbox(
                GetLoc("menu.npc_attack_direction_weapon_swing", "Resolve NPC attack at weaponSwing instead of preHitFrame"),
                &NPCAttackDirectionAtWeaponSwing)) {
            changed = true;
        }
        ImGuiMCP::SetNextItemWidth(280.0f);
        if (ImGuiMCP::SliderInt(
                GetLoc("menu.npc_attack_direction_fallback", "NPC attack direction when unknown (0 = keep unknown)"),
                &NPCAttackDirectionFallback, 0, 8)) {
            changed = true;
        }

        if (changed) {
            SaveSettings();
        }
    }

    void Register()
    {
        LoadLanguage();
        LoadSettings();

        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }

        SKSEMenuFramework::SetSection("DMK");
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.settings", "Settings"), MSettings);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.legacy_converter", "Legacy Converter"), RenderMenu);
        SKSE::log::info("OAR Converter UI & Settings Registered successfully.");
    }
}
