idf_component_register(SRCS "ModFile.cpp" "main.c" "font-5x7.c" "audbrd_bus.c" "audbrd_chardisp.c" "audbrd.c" "PanelTracker.cpp" "Tracker.cpp" "ModFile.cpp"
					EMBED_FILES audsch.bit
                    INCLUDE_DIRS ""
					PRIV_REQUIRES fatfs wear_levelling esp_partition)
