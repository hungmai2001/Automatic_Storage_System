import asyncio
import websockets
import os
import subprocess
import time
import shutil
import re

name_image = "image_store_"
# Đường dẫn đến thư mục cần kiểm tra
folder_to_check = "D:/DATN/esp32_cam_image"
# Tên của tệp cần kiểm tra
name_to_check = "image"
# Thư mục để lưu trữ khuôn mặt cần lưu
folder_store = "D:/DATN/esp32_cam_image/img_store"
# Tạo thư mục nếu nó không tồn tại
os.makedirs(folder_store, exist_ok=True)
image_count = 0

async def send_signal(signal):
    uri = "ws://192.168.1.11:80/ws"
    try:
        async with websockets.connect(uri) as websocket:
            if websocket.open:
            # Gửi tín hiệu đến server
                await websocket.send(signal)
                print(f"Sent signal: {signal}")
            else:
                print("Failed to connect to WebSocket server.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Đảm bảo đóng kết nối sau khi gửi xong
        if websocket and websocket.open:
            await websocket.close()
        print("WebSocket connection closed.")

def find_unknown_people(filename):
    
        filepath = os.path.join(folder_to_check, filename)

        # Kiểm tra xem tệp có phải là ảnh không
        if os.path.isfile(filepath) and filename.lower().endswith(('.png', '.jpg', '.jpeg')):
            # Chạy lệnh face_recognition
            print(f"folder_store: {folder_store}")
            print(f"filepath: {filepath}")
            command = f"face_recognition --tolerance 0.45 {folder_store} {filepath}"

            # Sử dụng subprocess để chạy lệnh và bắt kết quả đầu ra
            try:
                result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)

                print(f"result.stdout: {result.stdout}")
                count_occurrences = result.stdout.count(name_image)
                print(f"count_occurrences: {count_occurrences}")

                # Kiểm tra nếu có kết quả khớp trong đầu ra
                if count_occurrences == 1:
                    # Sử dụng regex để trích xuất các số sau "name_image"
                    matches = re.findall(f'{name_image}(\d+)', result.stdout)

                    for match in matches:
                        base_name, extension = os.path.splitext(filename)
                        new_file_name = f"{base_name}_{match}"
                        # Gửi tín hiệu đến WebSocket ở đây
                        new_file = f"{new_file_name}{extension}"
                        print(f"Person detected in {new_file}. Send signal to WebSocket.")
                        asyncio.get_event_loop().run_until_complete(send_signal(f"{new_file_name}"))
                        #Remove file
                        stdout = result.stdout.replace("\n", "")
                        new_file_remove = f"{stdout}{extension}"
                        file_remove = os.path.join(folder_store, new_file_remove)
                        print(f"remove file {file_remove}")
                        os.remove(file_remove)
                elif count_occurrences >= 1:
                    print("So many people!!!!!")
                else:
                    print(f"No person detected in {filename}.")
                    asyncio.get_event_loop().run_until_complete(send_signal("no_person"))
            except subprocess.CalledProcessError as e:
                # Lệnh thất bại, không có người nào được tìm thấy
                print(f"Error processing {filename}: {e}")
            #Remove file
            os.remove(filepath)

def check_file_in_folder():
    # Lấy danh sách các tệp trong thư mục
    files = os.listdir(folder_to_check) 
    for file in files:
        if name_to_check in file:
            print(f"Name '{name_to_check}' found in the folder. Exiting.")
            return file        
    print(f"Name '{name_to_check}' not found in the folder.")
    return None

def count_images_in_folder(folder_path):
    # Đếm số lượng tệp hình trong thư mục
    image_files = [f for f in os.listdir(folder_path) if f.lower().endswith(('.png', '.jpg', '.jpeg'))]
    return len(image_files)

def move_image(file_store):
    filepath = os.path.join(folder_to_check, file_store)

    if os.path.exists(filepath):
        # Đếm số lượng hình trong thư mục lưu trữ
        image_count = count_images_in_folder(folder_store)

        # Thêm số lượng hình vào tên file
        base_name, extension = os.path.splitext(file_store)
        new_file_name = f"{base_name}_{image_count + 1}{extension}"

        destination_path = os.path.join(folder_store, new_file_name)
        # Di chuyển tệp đến thư mục lưu trữ
        shutil.move(filepath, destination_path)
        print(f"File {file_store} moved to {folder_store}.")
    else:
        print(f"No image in {folder_to_check}.")

if __name__ == "__main__":
    # Vòng lặp vô hạn
    while (True):
        file_check = check_file_in_folder()
        if file_check:
            if "image_store" in file_check:
                move_image(file_check)
            elif "image_get" in file_check:
                find_unknown_people(file_check)
        # Chờ 2 giây trước khi kiểm tra lại
        else:
            time.sleep(2)
