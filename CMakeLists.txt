cmake_minimum_required(VERSION 3.22)
project(PS2LAN_TUNNEL VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(P2LT_BUILD_CAPTURE "Habilitar captura de quadros Ethernet via Npcap" ON)
option(P2LT_BUILD_TESTS "Habilitar a compilacao dos testes e o registro no CTest" ON)

if(MSVC)
  add_compile_options(/utf-8 /W3 /wd4996)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_compile_definitions(_WIN32_WINNT=0x0601)

if(P2LT_BUILD_CAPTURE)
  find_package(Npcap)
endif()

file(GLOB_RECURSE P2LT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

list(APPEND P2LT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# ---- Executavel principal ------------------------------------------------------------
# O alvo ps2lan precisa de src/main.cpp (ponto de entrada), que ainda nao existe
# nesta etapa do projeto. Sem o guard abaixo, add_executable() aborta a geracao
# de todo o projeto e impede ate a validacao dos testes.
#
# O guard e estritamente sobre a EXISTENCIA do arquivo: nao remove nenhuma fonte,
# nao inventa main.cpp e nao altera a lista P2LT_SOURCES. Assim que
# src/main.cpp for criado, o alvo passa a ser gerado normalmente, sem nenhuma
# edicao adicional neste arquivo.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  add_executable(ps2lan "${P2LT_SOURCES}")

  target_include_directories(ps2lan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

  target_link_libraries(ps2lan PRIVATE ws2_32 iphlpapi bcrypt)

  if(P2LT_BUILD_CAPTURE AND P2LT_NPCAP_FOUND)
    target_include_directories(ps2lan PRIVATE "${P2LT_NPCAP_INCLUDE_DIR}")
    target_link_libraries(ps2lan PRIVATE "${P2LT_NPCAP_WPCAP_LIB}")
    target_compile_definitions(ps2lan PRIVATE P2LT_HAS_PCAP=1)
    set_target_properties(ps2lan PROPERTIES
      VS_DEBUGGER_ENVIRONMENT "PATH=${P2LT_NPCAP_DLL_DIR};$ENV{PATH}")
  endif()

  install(TARGETS ps2lan RUNTIME DESTINATION bin)
else()
  message(STATUS
    "P2LT: src/main.cpp ainda nao existe -- o executavel 'ps2lan' NAO foi criado nesta configuracao.")
  message(STATUS
    "P2LT: isto e esperado e NAO e um erro; crie src/main.cpp para habilitar o alvo 'ps2lan'.")
  if(P2LT_BUILD_TESTS)
    message(STATUS
      "P2LT: os testes permanecem habilitados e independem do executavel principal.")
  endif()
endif()

# ---- Testes ---------------------------------------------------------------------------
# P2LT_BUILD_TESTS e ON por padrao, coerente com P2LT_BUILD_CAPTURE. Desative com
# -DP2LT_BUILD_TESTS=OFF para gerar apenas o executavel principal.
#
# Os testes sao deliberadamente SEPARADOS do executavel ps2lan:
#   - O alvo abaixo declara explicitamente suas proprias fontes. O glob de src/
#     acima varre apenas src/, portanto tests/ nunca entra em ps2lan e o main()
#     do teste nao pode ser absorvido pelo executavel principal.
#   - Somente o codec e_autocontido: nao linka ws2_32, iphlpapi, bcrypt ou
#     Npcap, porque o codigo sob teste nao usa nenhuma dessas APIs.
#   - C++17 vem de CMAKE_CXX_STANDARD, definido no topo deste arquivo.
if(P2LT_BUILD_TESTS)
  enable_testing()

  add_executable(test_p2p_protocol
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_p2p_protocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/net/p2p_protocol.cpp"
  )

  target_include_directories(test_p2p_protocol PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

  add_test(NAME p2p_protocol COMMAND test_p2p_protocol)
endif()