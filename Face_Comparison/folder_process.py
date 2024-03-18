import subprocess
import os
import shutil
import re
import asyncio
import websockets

# Gửi data đến websocket
async def send_signal(signal):
    uri = "ws://192.168.1.8:80/ws"
    websocket = await websockets.connect(uri)
    try:
        # Gửi tín hiệu đến server
        await websocket.send(signal)
        print(f"Sent signal: {signal}")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Đảm bảo đóng kết nối sau khi gửi xong
        await websocket.close()
        print("WebSocket connection closed.")

# Tìm khuôn mặt người trong folder và gửi kết quả đến ESP32-CAM, remove hình ảnh
def find_unknown_people(filename, folder_store, folder_to_check, name_image):
        filepath = os.path.join(folder_to_check, filename)
        # Kiểm tra xem tệp có phải là ảnh không
        if os.path.isfile(filepath) and filename.lower().endswith(('.png', '.jpg', '.jpeg')):
            # Chạy lệnh face_recognition
            print(f"folder_store: {folder_store}")
            print(f"filepath: {filepath}")
            command = f"face_recognition --tolerance 0.45 {folder_store} {filepath}"

            # Sử dụng subprocess để chạy lệnh và gửi kết quả đến ESP32-CAM
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
                        # Thay thế bằng mqtt
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
                    # Thay thế bằng mqtt
                    asyncio.get_event_loop().run_until_complete(send_signal("no_person"))
            except subprocess.CalledProcessError as e:
                # Lệnh thất bại, không có người nào được tìm thấy
                print(f"Error processing {filename}: {e}")
            # Remove file
            os.remove(filepath)

# Kiểm tra xem có ảnh trong download folder không
def check_file_in_folder(folder_to_check, name_to_check):
    # Lấy danh sách các tệp trong thư mục
    files = os.listdir(folder_to_check) 
    for file in files:
        if name_to_check in file:
            print(f"Name '{name_to_check}' found in the folder. Exiting.")
            return file
    print(f"Name '{name_to_check}' not found in the folder.")
    return None

# Xóa ảnh trong store folder
def remove_file_in_folder(folder_store):
    # Lấy danh sách các tệp trong thư mục
    files = os.listdir(folder_store) 
    for file in files:
        filepath = os.path.join(folder_store, file)
        os.remove(filepath)
        print(f"{filepath} deleted.")
    return None

# Đếm số lượng tệp hình trong thư mục lưu trữ
def count_images_in_folder(folder_store):
    image_files = [f for f in os.listdir(folder_store) if f.lower().endswith(('.png', '.jpg', '.jpeg'))]
    return len(image_files)

# Move ảnh đến store folder từ download folder
def move_image(file_store, folder_to_check, folder_store):
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
        return destination_path
    else:
        print(f"No image in {folder_to_check}.")
        return None