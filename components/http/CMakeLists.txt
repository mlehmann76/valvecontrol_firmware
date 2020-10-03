idf_component_register(SRCS 
	"DefaultHandler.cpp" 
	"HttpAuth.cpp"
	"HttpRequest.cpp"
	"HttpResponse.cpp"
	"HttpServer.cpp"
	"RequestHandlerBase.cpp"
	"utilities.cpp"
	"socket.cpp"
	INCLUDE_DIRS "."
	REQUIRES FreeRTOScpp mbedtls wpa_supplicant )
