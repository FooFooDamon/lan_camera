alias lanc_client='screen -d -m -U -S lanc_client -t lanc_client -h 100000 -L -Logfile /tmp/lanc_client.log bash -c "time lanc_client -c ~/etc/lan_camera.cli.json"'
alias lanc_server='screen -d -m -U -S lanc_server -t lanc_server -h 100000 -L -Logfile /tmp/lanc_server.log bash -c "time lanc_server -c ~/etc/lan_camera.srv.json"'

