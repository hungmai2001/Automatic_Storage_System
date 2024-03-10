
from pymongo.mongo_client import MongoClient
from gridfs import GridFS
import os

uri = "mongodb+srv://hungmai2001:Xuanhung123@cluster0.3gjdvmd.mongodb.net/?retryWrites=true&w=majority"

# Create a new client and connect to the server
client = MongoClient(uri)

# Chọn hoặc tạo một database mới
db = client["MongoDB_image"]

# Chọn hoặc tạo GridFS
fs = GridFS(db, collection="images")
# Đường dẫn đến thư mục chứa hình ảnh
image_folder = "D:/DATN/esp32_cam_image/img_store"
# Tạo thư mục để lưu hình ảnh
output_folder = "D:/DATN/esp32_cam_image/img_get"
os.makedirs(output_folder, exist_ok=True)

def push_image():
    # Lặp qua tất cả các hình ảnh trong thư mục và tải lên MongoDB
    for root, dirs, files in os.walk(image_folder):
        for file in files:
            file_path = os.path.join(root, file)
            with open(file_path, "rb") as f:
                # Lưu trữ hình ảnh trong GridFS
                fs.put(f, filename=file)

    print("Upload completed.")

def get_image():
    # Lấy danh sách tất cả các hình ảnh từ GridFS
    image_list = list(fs.find())
    collections_list = db.list_collection_names()
    print("Danh sách các collections:", collections_list)
    print("get_image", image_list)
    # Lặp qua danh sách và lưu từng hình ảnh xuống thư mục img_get
    for image_info in image_list:
        image_id = image_info._id
        image_data = fs.get(image_id).read()

        # Tạo đường dẫn để lưu hình ảnh
        output_path = os.path.join(output_folder, image_info.filename)

        # Lưu hình ảnh xuống thư mục img_get
        with open(output_path, "wb") as output_file:
            output_file.write(image_data)

        print(f"Hình ảnh {image_id} đã được lưu tại {output_path}")

def delete_image(filename):
    # Tìm thông tin về hình ảnh trong GridFS
    image_info = fs.find_one({"filename": filename})

    if image_info:
        image_id = image_info._id
        # Xóa hình ảnh từ GridFS
        fs.delete(image_id)
        print(f"Hình ảnh {filename} đã bị xóa thành công.")
    else:
        print(f"Không tìm thấy hình ảnh có tên {filename} trong GridFS.")

if __name__ == "__main__":
    # push_image()
    #get_image()
    delete_image("image_store_3.png")