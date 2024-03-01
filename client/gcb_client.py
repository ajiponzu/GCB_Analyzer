import os
import requests
import time

def request_analyze_video(video_path: str, request_json_file: str):
    analyzed_video_file = None
    with open(video_path, 'rb') as fp:
        analyzed_video_file = fp.read()

    #request_json_file = None
    #with open(request_json_path, 'r') as fp:
    #    request_json_file = fp.read()

    video_file = os.path.basename(video_path)
    url = f"http://127.0.0.1:8080/analyze_video/{video_file}"
    connect_status = requests.post(
        url=url, files={"video": analyzed_video_file, "request_json": request_json_file})

    print(connect_status)
    json_data = connect_status.json()
    result_access_id = json_data.get("access_id")

    if result_access_id is None:
        print(json_data.get("error"))
        return -1, None

    print('result_access_id=',result_access_id)
    time.sleep(1.0)

    while True:
        url = f"http://127.0.0.1:8080/analyzation_result/{result_access_id}"
        connect_status = requests.get(url=url)

        json_data = connect_status.json()

        error_msg = json_data.get("error")
        if error_msg is not None:
            print(error_msg)
            return -1, None

        process_progression = json_data.get("progression")
        if process_progression is None:
            break

        os.system("clear")
        print(f"process_id: {result_access_id}")
        print(f"progress: {int(process_progression * 100)}%")
        print("[" + "*" * int(process_progression * 50) + "]")
        time.sleep(1.2)

    os.system("clear")
    print(f"process_id: {result_access_id}")
    print(f"progress: {100}%")
    print("[" + "*" * int(1.0 * 50) + "]")

    #with open(f"./temp/result{result_access_id}.json", 'w') as fp:
    #    print(json_data, file=fp)

    return result_access_id, json_data


def request_visualize_analyzation_result(request_access_id, video_path: str):
    url = f"http://127.0.0.1:8080/visualize_analyzation_result/{request_access_id}/{video_path}"
    connect_status = requests.post(url=url)

    print(connect_status)
    json_data = connect_status.json()
    result_visualize_access_id = json_data.get("access_id")

    if result_visualize_access_id is None:
        print(json_data.get("error"))
        return None

    print(f"process_id: {result_visualize_access_id}")
    time.sleep(2.0)

    video_binary = None

    while True:
        url = f"http://127.0.0.1:8080/visualization_result/{result_visualize_access_id}"
        connect_status = requests.get(url=url)

        content_type = connect_status.headers["Content-Type"]
        if not "application/json" in content_type:
            video_binary = connect_status.content
            break

        json_data = connect_status.json()

        error_msg = json_data.get("error")
        if error_msg is not None:
            print(error_msg)
            return None

        process_progression = json_data.get("progression")

        os.system("clear")
        print(f"process_id: {result_visualize_access_id}")
        print(f"progress: {int(process_progression * 100)}%")
        print("[" + "*" * int(process_progression * 50) + "]")
        time.sleep(1.2)

    os.system("clear")
    print(f"process_id: {result_visualize_access_id}")
    print(f"progress: {100}%")
    print("[" + "*" * int(1.0 * 50) + "]")

    return video_binary
