set(P2LT_NPCAP_FOUND FALSE)

set(_pcap_hint_dirs
  "$ENV{NPcapSDK}"
  "$ENV{NpcapSDK}"
  "$ENV{NPCAP_SDK}"
  "C:/Program Files/Npcap"
  "C:/Program Files/Npcap/SDK"
  "C:/npcap-sdk"
)

find_path(P2LT_NPCAP_INCLUDE_DIR
  NAMES pcap.h
  HINTS ${_pcap_hint_dirs}
  PATH_SUFFIXES Include include inc
)

if(P2LT_NPCAP_INCLUDE_DIR)
  find_path(_npcap_lib_dir
    NAMES wpcap.lib
    HINTS
      "${P2LT_NPCAP_INCLUDE_DIR}/Lib/x64"
      "${P2LT_NPCAP_INCLUDE_DIR}/lib/x64"
      "${P2LT_NPCAP_INCLUDE_DIR}/Lib"
      "${P2LT_NPCAP_INCLUDE_DIR}/lib"
    NO_DEFAULT_PATH
  )
  find_path(_npcap_dll_dir
    NAMES wpcap.dll
    HINTS
      "C:/Windows/System32/Npcap"
      "C:/Program Files/Npcap"
    NO_DEFAULT_PATH
  )
  if(_npcap_lib_dir)
    set(P2LT_NPCAP_WPCAP_LIB "${_npcap_lib_dir}/wpcap.lib")
    if(EXISTS "${_npcap_lib_dir}/Packet.lib")
      set(P2LT_NPCAP_PACKET_LIB "${_npcap_lib_dir}/Packet.lib")
    endif()
    if(_npcap_dll_dir)
      set(P2LT_NPCAP_DLL_DIR "${_npcap_dll_dir}")
    endif()
    set(P2LT_NPCAP_FOUND TRUE)
  endif()
endif()

if(NOT P2LT_NPCAP_FOUND)
  message(WARNING
    "Npcap SDK nao encontrado. O executavel sera compilado SEM captura de "
    "quadros Ethernet (build de transporte/diagnostico apenas). "
    "Baixe o SDK em https://npcap.com/#download e defina a variavel de "
    "ambiente NPcapSDK apontando para a pasta do SDK (ex: C:/npcap-sdk). "
    "Scripts auxiliares: scripts/setup_npcap.ps1.")
endif()