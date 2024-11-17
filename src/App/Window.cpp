#include "Window.h"

#include <QMouseEvent>
#include <QLabel>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVBoxLayout>
#include <QScreen>
#include <QSlider>

#include <array>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tinygltf/tiny_gltf.h>

namespace
{

constexpr std::array<GLfloat, 8> vertices = {
	-1.0f, 1.0f,
	-1.0f, -1.0f,
	1.0f, -1.0f,
	1.0f, 1.0f,
};
constexpr std::array<GLuint, 6u> indices = {0, 1, 2, 0, 2, 3};

}// namespace

Window::Window() noexcept
{
	const auto formatFPS = [](const auto value) {
		return QString("FPS: %1").arg(QString::number(value));
	};

	auto fps = new QLabel(formatFPS(0), this);
	fps->setStyleSheet("QLabel { color : white; }");

	auto iters_slider = new QSlider(Qt::Horizontal);
	iters_slider->setRange(100, 10000);
	iters_slider->setValue(500);
	connect(iters_slider, &QSlider::valueChanged, this, [this](float value) { max_iterations = value; });

	auto iters_label = new QLabel("Iters: 0", this);
	iters_label->setStyleSheet("QLabel { color : white; }");

	auto border_slider = new QSlider(Qt::Horizontal);
	border_slider->setRange(0, 1000);
	border_slider->setValue(200);
	connect(border_slider, &QSlider::valueChanged, this, [this](float value) { border = value / 100.0f; });

	auto border_label = new QLabel("Border: 0", this);
	border_label->setStyleSheet("QLabel { color : white; }");

	auto layout = new QVBoxLayout();
	layout->addWidget(fps, 1);
	layout->addWidget(iters_label);
	layout->addWidget(iters_slider);
	layout->addWidget(border_label);
	layout->addWidget(border_slider);
	
	setLayout(layout);
	
	timer_.start();
	
	connect(this, &Window::updateUI, [=] {
		fps->setText(formatFPS(ui_.fps));
		iters_label->setText(QString("Iters: %1").arg(max_iterations));
		border_label->setText(QString("Border: %1").arg(border));
	});
}

Window::~Window()
{
	{
		// Free resources with context bounded.
		const auto guard = bindContext();
		program_.reset();
	}
}

void Window::onInit()
{
	// Configure shaders
	program_ = std::make_unique<QOpenGLShaderProgram>(this);
	program_->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/Shaders/diffuse.vs");
	program_->addShaderFromSourceFile(QOpenGLShader::Fragment,
									  ":/Shaders/diffuse.fs");
	program_->link();

	// Create VAO object
	vao_.create();
	vao_.bind();

	// Create VBO
	vbo_.create();
	vbo_.bind();
	vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo_.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(GLfloat)));

	// Create IBO
	ibo_.create();
	ibo_.bind();
	ibo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	ibo_.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(GLuint)));

	// Bind attributes
	program_->bind();

	program_->enableAttributeArray(0);
	program_->setAttributeBuffer(0, GL_FLOAT, 0, 2, static_cast<int>(2 * sizeof(GLfloat)));

	mvpUniform_ = program_->uniformLocation("mvp");
	screenposUniform_ = program_->uniformLocation("screen_pos");
	screenscaleUniform_ = program_->uniformLocation("screen_scale");
	maxitersUniform_ = program_->uniformLocation("max_iterations");
	borderUniform_ = program_->uniformLocation("border2");

	// Release all
	program_->release();

	vao_.release();

	ibo_.release();
	vbo_.release();

	// Еnable depth test and face culling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Clear all FBO buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::onRender()
{
	const auto guard = captureMetrics();

	// Clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Calculate MVP matrix
	model_.setToIdentity();
	view_.setToIdentity();
	const auto mvp = projection_ * view_ * model_;

	// Bind VAO and shader program
	program_->bind();
	vao_.bind();

	// Update uniform value
	program_->setUniformValue(mvpUniform_, mvp);

	QVector4D pos4(pos_.x(), pos_.y(), 0, 1);
	QVector4D pos4_new = mvp * pos4;
	QVector2D pos2(pos4_new.x(), pos4_new.y());
	pos2 /= QVector2D(width(), height());
	fixWindowRatio(pos2);

	program_->setUniformValue(screenposUniform_, pos2);
	program_->setUniformValue(screenscaleUniform_, scale_);
	program_->setUniformValue(maxitersUniform_, static_cast<int>(max_iterations));
	program_->setUniformValue(borderUniform_, border * border);

	// Draw
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

	// Release VAO and shader program
	vao_.release();
	program_->release();

	++frameCount_;

	// Request redraw if animated
	if (animated_)
	{
		update();
	}
}

void Window::onResize(const size_t width, const size_t height)
{
	// Configure viewport
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	
	// Configure matrix
	const auto aspect = static_cast<float>(width) / static_cast<float>(height);
	const auto zNear = 0.1f;
	const auto zFar = 100.0f;
	const auto fov = 45.0f;
	projection_.setToIdentity();
	if (aspect > 1.0f)
	{
		projection_.ortho(-1.0f * aspect, 1.0f * aspect, -1.0f, 1.0f, zNear, zFar);
	}
	else
	{
		projection_.ortho(-1.0f, 1.0f, -1.0f / aspect, 1.0f / aspect, zNear, zFar);
	}
}

void Window::mousePressEvent(QMouseEvent * e)
{
	dragging_ = true;
	lastMousePos_ = e->pos();
}

void Window::mouseMoveEvent(QMouseEvent * e)
{
	if (dragging_)
	{
		QVector2D delta = QVector2D(e->pos() - lastMousePos_);
		fixWindowRatio(delta);
		delta.setX(-delta.x());

		pos_ += delta * scale_ * 2.0f;

		lastMousePos_ = e->pos();
	}
}

void Window::mouseReleaseEvent(QMouseEvent *)
{
	dragging_ = false;
}

void Window::wheelEvent(QWheelEvent * e)
{
	float scaleFactor = e->angleDelta().y() < 0 ? 0.1f : -0.1f;

	QVector2D center(width() * 0.5f, height() * 0.5f);
	QVector2D mouseVec = QVector2D(e->position()) - center;

	mouseVec.setX(-mouseVec.x());
	fixWindowRatio(mouseVec);

	pos_ += mouseVec * scaleFactor * scale_ * 2;

	scale_ *= (1.0f + scaleFactor);

}

void Window::keyPressEvent(QKeyEvent * event)
{
	if (event->key() == Qt::Key_Escape) {
		close();
	}
}

void Window::fixWindowRatio(QVector2D & pos) const
{
	float aspectRatio = static_cast<float>(width()) / height();
	if (aspectRatio > 1)
	{
		pos.setX(pos.x() * aspectRatio);
	}
	else
	{
		pos.setY(pos.y() / aspectRatio);
	}
}

Window::PerfomanceMetricsGuard::PerfomanceMetricsGuard(std::function<void()> callback)
	: callback_{ std::move(callback) }
{
}

Window::PerfomanceMetricsGuard::~PerfomanceMetricsGuard()
{
	if (callback_)
	{
		callback_();
	}
}

auto Window::captureMetrics() -> PerfomanceMetricsGuard
{
	return PerfomanceMetricsGuard{
		[&] {
			if (timer_.elapsed() >= 1000)
			{
				const auto elapsedSeconds = static_cast<float>(timer_.restart()) / 1000.0f;
				ui_.fps = static_cast<size_t>(std::round(frameCount_ / elapsedSeconds));
				frameCount_ = 0;
				emit updateUI();
			}
		}
	};
}
