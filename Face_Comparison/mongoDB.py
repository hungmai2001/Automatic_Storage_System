import os

def push_image(file_path, fs, file_name):
    # Lặp qua tất cả các hình ảnh trong thư mục và tải lên MongoDB
    with open(file_path, "rb") as f:
        # Lưu trữ hình ảnh trong GridFS
        fs.put(f, filename=file_name)
        print(f"Upload {file_name} completed.")

def get_image_from_database(fs,db,output_folder):
    # Lấy danh sách tất cả các hình ảnh từ GridFS
    image_list = list(fs.find())
    collections_list = db.list_collection_names()
    print("Danh sách các collections:", collections_list)
    print("get_image", image_list)
    # Lặp qua danh sách và lưu từng hình ảnh xuống thư mục img_get
    for image_info in image_list:
        image_id = image_info._id
        image_data = fs.get(image_id).read()
        image_name = image_info.filename
        # Tạo đường dẫn để lưu hình ảnh
        output_path = os.path.join(output_folder, image_info.filename)
        # Lưu hình ảnh xuống thư mục img_get
        with open(output_path, "wb") as output_file:
            output_file.write(image_data)

        print(f"Hình ảnh {image_name} đã được lưu tại {output_path}")

def delete_image(filename,fs):
    # Tìm thông tin về hình ảnh trong GridFS
    image_info = fs.find_one({"filename": filename})

    if image_info:
        image_id = image_info._id
        # Xóa hình ảnh từ GridFS
        fs.delete(image_id)
        print(f"Hình ảnh {filename} đã bị xóa thành công.")
    else:
        print(f"Không tìm thấy hình ảnh có tên {filename} trong GridFS.")